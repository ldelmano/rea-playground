# REA 6.4.0 evaluation: Catima 2.45.1 (me.hackerchick.catima), /opt/apk/catima_1004.apk

All evidence comes from rea output (jadx 0.7.1 bridge and apktool 3.0.3). No source, web, jadx or apktool was used directly. I never saw text in REA output aimed at an AI.

**Suspicious text:** none found.

## a. Barcode generation and reading

**Libraries.**
- ZXing core, 139 `com.google.zxing.*` classes. `search-android-classes ""` lists them.
- journeyapps zxing-android-embedded for the live camera scan: `BarcodeView`, `DecoderThread`, `CaptureManager`, `DecoratedBarcodeView`. The apktool string `library_zxingandroidembedded_repositoryLink` confirms it.
- Catima wrappers: `protect.card_locker.CatimaBarcode` (wraps `com.google.zxing.BarcodeFormat`, with `fromName` and `prettyName`) and `CatimaCaptureManager`.

**Generation.**
- `BarcodeImageWriterTask.generate` (`inspect-android-method`) calls `encode(cardId, catimaBarcode.mBarcodeFormat, w, h, hints)`.
- It passes `EncodeHintType.CHARACTER_SET` only when the charset is not ISO-8859-1.
- It turns the returned `BitMatrix` into an ARGB bitmap, then scales it by an integer factor.
- R8 merged all the zxing writers into `androidx.transition.Transition.AnonymousClass1`, selected by `$r8$classId`. Its `case 14` switches on `barcodeFormat.ordinal()`.
- The formats I mapped from the decompiled ordinals are below. I assumed standard ZXing enum order.

| Ordinal | Writer | Format |
|---|---|---|
| 0 | Aztec | AZTEC |
| 1 | `CodaBarWriter` | CODABAR |
| 2 | `ITFWriter(2)` | CODE_39 |
| 3 | `ITFWriter(3)` | CODE_93 |
| 4 | `ITFWriter(1)` | CODE_128 |
| 5 | DataMatrix | DATA_MATRIX |
| 6, 7 | `EAN8Writer` variants | EAN_8, EAN_13 |
| 8 | `ITFWriter(0)` | ITF |
| 10 | `PDF417Writer` | PDF_417 |
| 11 | merged class (default branch) | QR_CODE |
| 14 | `UPCAWriter` | UPC_A |
| 15 | `EAN8Writer` variant | UPC_E |

- Ordinals 9, 12 and 13 (MAXICODE, RSS_14, RSS_EXPANDED) fall to the default branch and log "No encoder available for format".
- So the app can generate 13 formats: AZTEC, CODABAR, CODE_39, CODE_93, CODE_128, DATA_MATRIX, EAN_8, EAN_13, ITF, PDF_417, QR_CODE, UPC_A, UPC_E.

**Reading.** Three paths exist.
1. Live camera: `ScanActivity.onCreate` builds a `CatimaCaptureManager` and calls `barcodeView.startDecoderThread()`. `MultiFormatReader` is referenced by `Decoder`, `DecoderThread` and `BarcodeView` (`trace-android-references`).
2. From images: `Utils.retrieveBarcodesFromImage` and `Utils.getBarcodesFromBitmapReal`. The latter uses `GenericMultipleBarcodeReader(new MultiFormatReader())` with hint `ALSO_INVERTED=true` and no format restriction. It returns `ParseResult(BARCODE_ONLY, ...)` objects.
3. From PDF: `Utils.retrieveBarcodesFromPdf` renders each page with `android.graphics.pdf.PdfRenderer` and scans the bitmap. It also reads `PkPass` and `PkPasses` files: `Utils.retrieveBarcodesFromPkPass(es)` and `PkpassParser.parsePassJSONBarcodeField`, which maps `PKBarcodeFormatQR` and similar names to `CatimaBarcode`.

Because no format restriction is visible, reading appears to accept every format the ZXing readers in the APK support. Reader classes present: Aztec, DataMatrix, MaxiCode, PDF417, Codabar, Code39, Code93, Code128, ITF, UPC/EAN, RSS-14, RSS-Expanded.

**Confidence:** high for the libraries and the generation path, medium for the exact format-to-writer mapping, medium for the claim that reading is unrestricted.

**REA could not show:**
- A readable list of formats. `CatimaBarcode.<clinit>` came back with no decompiled body, so the "supported" list in the UI (`barcodeFormats`, `barcodePrettyNames`) was not visible.
- Whether the camera decoder factory sets a hint set. Only `onCreate` could be searched, and it is mostly view-binding code.

## b. Import from other apps

**Sources.** The `chooseImportType` dialog and `DataFormat` enum have three entries: Catima, Fidme and VoucherVault.
- `ImportExportActivity.chooseImportType` adds " (BETA)" to the "Fidme" entry.
- The strings `importCatima`, `importFidme` and `importVoucherVault` confirm the three sources, via `decode-android-resources`.

**Flow.**
1. Manifest `VIEW` filters on `content:` for `application/zip`, `application/json` and `text/comma-separated-values` (labelled `@string/importCards`) route to `ImportExportActivity`. In `onCreate`, `chooseImportType(intent.getData())` shows the dialog and the file picker (`fileOpenLauncher`).
2. A new `Thread` runs `ImportExportTask`.
3. `ImportExportTask.call` creates `new DBHelper(activity).getWritableDatabase()` and switches on `DataFormat.ordinal()`: 0 gives `CatimaImporter`, 1 gives the Fidme importer, 2 gives `VoucherVaultImporter`.
4. `Utils.copyToTempFile(..., "MultiFormatImporter.zip")` copies the input.
5. It runs `beginTransaction()`, then `importer.importData(context, db, file, password)`, then `setTransactionSuccessful()`.
6. A `net.lingala.zip4j` `ZipException` of the wrong-password type is reported as `BadPassword`.

**Parsers.**
- `CatimaImporter.importData` reads a zip (zip4j, optional password) containing `catima.csv` and `card_<id>_front|back|icon.png`. If the input is not a zip it falls back to plain CSV. Parsing uses Apache `commons-csv`: `importCSV` and `importLoyaltyCard`, with columns `_id, store, note, validfrom, expiry, balance, balancetype, cardid, barcodeid, barcodetype, barcodeencoding, headercolor, starstatus, archive, lastused`. It also has `parseV2Cards`, `parseV2Groups` and `parseV2CardGroups` for the old format.
- Fidme importer: it is `androidx.transition.Transition.AnonymousClass1.importData`, an R8-merged class that REA only revealed through `trace-android-references` on `Importer`. It reads `loyalty_programs.csv` from a (possibly encrypted) zip, with a `;`-delimited `CSVFormat.RFC4180` parser. `importLoyaltyCard` builds the cards.
- `VoucherVaultImporter.importData` reads a JSON array with fields `description`, `expires`, `balance`/`balanceMilliunits`, `code`, `codeType` (PDF417, QR, EAN13, CODE128, CODE39, TEXT) and `color`.
- Database writes: `DBHelper.insertLoyaltyCard(db, store, note, validFrom, expiry, balance, balanceType, cardId, barcodeId, barcodeType, barcodeEncoding, headerColor, starStatus, lastUsed, archiveStatus)`. It uses `ContentValues` keys such as `store`, `note` and `validfrom`.

**Other import paths in `ScanActivity` / `MainActivity`.**
- Share URLs: `ImportURIHelper.parse` handles `catima.app/share` (host/path strings from the apktool strings dump, plus `/Catima/share` for thelastproject) and reads `store`, `note`, `cardid`, `barcodeid`, `barcodetype`, `barcodeencoding`, `balance`, `balancetype` and more from the query or fragment.
- Image, PDF and `.pkpass`/`.pkpasses` files, shared into `ScanActivity` via `SEND`/`VIEW` filters (`image/*`, `application/pdf`, pkpass MIME types).

**Confidence:** high for the Catima and VoucherVault flows, medium for the Fidme flow. Its decompiled loop has a missing `break` (a decompilation artifact), and I never saw the Fidme CSV column names.

**REA could not show:**
- The `import_types_array` contents directly. I inferred them from `DataFormat` and the strings.
- The `ImportExportActivity` lambdas, which `Utils$$ExternalSyntheticLambda1` and `LegacySenderService$$ExternalSyntheticLambda0` obscure (R8 merged them into unrelated class names).

## c. Permissions and where they are used

The manifest lists these. Both `inspect-android-package` (pkg.json) and `decode-android-resources` agree, but the first omits attributes such as `maxSdkVersion`.

| Permission | Where it is used | Confidence |
|---|---|---|
| `CAMERA` | `ScanActivity.onResume` (`checkSelfPermission` on `android.permission.CAMERA`, then `showCameraError`/`setCameraErrorState`); `com.journeyapps.barcodescanner.CaptureManager.onResume` does `ActivityCompat.requestPermissions(..., {CAMERA}, 250)`; result handled in `ScanActivity.onRequestPermissionsResult` (code 250) | high |
| `READ_EXTERNAL_STORAGE` (maxSdk 23) | `kotlin.ExceptionsKt.requestStorageReadPermission(Activity, int)`; callers use codes 100, 101, 102 in the "add card another way" dialog click handler (a `ScanActivity$$ExternalSyntheticLambda7`) and handle results in `ScanActivity.onRequestPermissionsResult`. It only requests when SDK < 24 | high |
| `BLUETOOTH_CONNECT` | `wearos.WearSyncPermissionRequester.getRequiredPermissions` (adds it when SDK >= 31); `wearos.BluetoothServerService.onStartCommand` checks it before starting the server | high |
| `POST_NOTIFICATIONS` | `WearSyncPermissionRequester.getRequiredPermissions` (SDK >= 33); `BluetoothServerService.onStartCommand` creates the `catima_wear_bt` notification channel | high |
| `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_CONNECTED_DEVICE` | `BluetoothServerService.onStartCommand`: `startForeground(1001, notification, 16)` (16 = connected-device type) | high |
| `BLUETOOTH`, `BLUETOOTH_ADMIN` (maxSdk 30) | not located. Presumably legacy pre-31 counterparts, but I did not find usage code | low |
| `NFC` | `cardview.LoyaltyCardViewActivity.configurePausedNfc`: `NfcAdapter.enableReaderMode(...)` while a card is shown (setting `pref_disable_nfc_while_viewing_card`). It blocks NFC payments, not reading tags | high |
| custom `DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION` | defined by AndroidX (signature level); not Catima's own | medium |

- The app also defines its own `me.hackerchick.catima.READ_CARDS` permission (protectionLevel `dangerous`), which guards the exported `CardsContentProvider` (`readPermission`).
- `BluetoothServerService` also has a nested `AcceptThread`, which REA could not inspect (see "What was hard").

**REA could not show:** usage of `BLUETOOTH`/`BLUETOOTH_ADMIN`; the permission-request call sites for BLUETOOTH_CONNECT/POST_NOTIFICATIONS in the settings UI (I saw the permission list in `WearSyncPermissionRequester` but not who launches it).

## d. Network and telemetry

- **INTERNET permission: absent.** It is not in the permission lists from either `inspect-android-package` or the apktool manifest, and no `ACCESS_NETWORK_STATE` either. Without INTERNET the app cannot open network sockets over IP.
- No http(s) URLs in the apktool string dump except `https://journeyapps.com/` and the zxing-android-embedded GitHub link (library credits text) and the schema URL `http://schemas.android.com/apk/res/android`. The `catima.app` hostname exists only as a deep-link host for the `/share` import URL.
- Class search found no okhttp, retrofit, firebase, crashlytics, sentry, analytics or volley classes except `com.yalantis.ucrop.OkHttpClientStore` (uCrop image cropper, a tiny stub with no okhttp classes in the APK).
- **ACRA** (crash reporter) is present (`org.acra.*`). `LoyaltyCardLockerApplication.onCreate` configures it with `MailSenderConfigurationBuilder`, `mailTo = "acra-crash@catima.app"` and a dialog. The only sender classes are `EmailIntentSender`, `NullSender`, `JobSenderService` and `LegacySenderService`. The settings string `pref_enable_acra_summary` says "Crash reports are never sent automatically." So reporting is a user-approved email intent, not HTTP.
- Bluetooth (`BluetoothServerService`, Wear OS sync, UUID in `WearBluetoothProtocol`, tokens in `WearBluetoothSecurity`) is the only wireless link, and it only serves the user's own paired watch.
- **Confidence:** high that there is no INTERNET permission and no analytics SDK; medium for "no network code at all". I could not scan every string constant in the DEX, and `trace-android-references` rejected framework classes (`java.net.HttpURLConnection`, `java.net.URL`, `ConnectivityManager`), so I could not search for their callers.

## Counts, time, usability

- **Total rea calls: about 80** (65 through my helper script, plus about 15 direct). Several were errors from my own wrong class names.
- **Wall time:** about 27 minutes, including my analysis. A rea call took about 5-8 s when run alone, not the 15 s stated.
- **Network activity by REA itself:** I did not monitor traffic, and saw nothing odd in its output. Each call copies the APK to `/tmp/rea-android-*` and starts a JVM.
- **Output quality.**
  - Good: `search-android-classes` filters properly (the `normalized_result.matches` field), and `inspect-android-method` returns usable decompiled Java. Each call carries an evidence ID, SHA, and a "limitations" list that honestly says it is static and never runs the APK.
  - Misleading or noisy: the `raw_result` of `search-android-classes` always dumps all 3292 classes even for a narrow query. The `normalized_result` is the useful part, but a raw/last-call read shows the full dump (about 300 KB).
  - R8-obfuscated code makes names misleading. `Importer.importCSV` returns `WearSyncPermissionRequester` (class-name reuse), the Fidme importer and all zxing writers live in `androidx.transition.Transition.AnonymousClass1`, and `ImportURIHelper` mixes unrelated methods. REA shows these names faithfully but does not warn.
  - Decompile artifacts: the Fidme `importData` has a `while(true)` read loop without a break, `CatimaBarcode.<clinit>` showed "no decompiled body", and lots of "Code duplicated" warnings in the `encode` method.
- **What was hard.**
  - There is no string search or cross-reference for constants (permission strings) in the Android toolset; I found usages by guessing likely methods and grepping method bodies.
  - `trace-android-references` only works for classes inside the APK, not framework classes (it returns `invalid_request`).
  - Inner classes (for example `BluetoothServerService$AcceptThread`) did not appear in the class search or inspect, though JADX's field types referenced them; lookup of `X$1` failed, and the real name was `X.AnonymousClass1`.
  - The CLI output is a large JSON envelope; I had to script the extraction.
- **Overall:** usable for a code reader who knows how to work around R8. Strong on evidence provenance and honest limits. Weak on discovery (no string search, no implementor lookup for interfaces except via `trace-android-references`).
