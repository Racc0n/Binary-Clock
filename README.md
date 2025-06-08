# Dokumenten-Service – Lokaler Setup & Testablauf

## Voraussetzungen

- **Node.js** ≥ 14 (mit npm)  
- **Docker Desktop** (für Windows/macOS) oder Docker Engine (Linux)  
- **Git Bash** oder eine POSIX-kompatible Shell (optional für `curl`)  
- **PowerShell** (für Windows-native Tests)  

---

## 1. MinIO als lokaler S3-Emulator aufsetzen

1. **Datenverzeichnis anlegen**:  
   ```bash
   mkdir "%USERPROFILE%\minio-data"
2. **MinIO-Container starten (Windows CMD/Powershell)**:
    ```powershell
    docker run -d --name minio `
    -p 9000:9000 -p 9001:9001 `
    -v "C:\Users\<DeinUser>\minio-data:/data" `
    -e "MINIO_ROOT_USER=minioadmin" `
    -e "MINIO_ROOT_PASSWORD=minioadmin" `
    minio/minio server /data --console-address ":9001"
3. **MinIO-Konsople öffnen unter http://localhost:9001 und amelden mit minioadmin / minioadmin**
4. **Bucket erstellen: test-bucket**

## 2. Projekt vorbereiten 

1. **Ordner wechseln**
    cd /Desktop/Document-Service
2. **Abhängigkeiten installieren:**
    *npm install express multer @aws-sdk/client-s3 sequelize sqlite3 axios dotenv*

3. **Verzeichnisstruktur prüfen:**
    Document-service/
    ├── src/
    │   ├── config/aws.js
    │   ├── controllers/
    │   ├── models/
    │   ├── services/
    │   ├── app.js
    │   └── index.js
    ├── tests/fixtures/sample.pdf
    ├── .env.local
    ├── package.json
    └── README.md

## 3. Umgebungsvariablen konfigurieren 

1. **Im Projekt-Root die Datei .env.local überprüfen (sollte schon so angelegt sein):**
    ```bash
    # MinIO (lokaler S3)
    S3_BUCKET=test-bucket
    AWS_ACCESS_KEY_ID=minioadmin
    S3_SECRET_ACCESS_KEY=minioadmin
    S3_ENDPOINT=http://localhost:9000
    S3_FORCE_PATH_STYLE=true
    # Antragsservice-Mock
    ANTRAGS_SERVICE_BASE_URL=http://localhost:5000
    # SQLite-Datenbank
    DB_STORAGE=./data/test-db.sqlite
    # Validierung
    MAX_FILE_SIZE=5242880
    ALLOWED_MIME_TYPES=image/jpeg,image/png,application/pdf
    ```
2. **src/config/aws.js überprüfen (sollte schon so angelegt sein):**
    ```javascript
    import { S3Client } from "@aws-sdk/client-s3";
    import dotenv from "dotenv";
    dotenv.config({ path: ".env.local" });
    export const s3Client = new S3Client({
    region: process.env.AWS_REGION,
    endpoint: process.env.S3_ENDPOINT,
    forcePathStyle: process.env.S3_FORCE_PATH_STYLE === "true",
    credentials: {
    accessKeyId: process.env.AWS_ACCESS_KEY_ID,
    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY
        }
    });
    export const bucketName = process.env.S3_BUCKET;
    ```

## 4. Mock-Antragsservice starten

1. **src/mock-antrag.js überprüfen (sollte schon so angelegt sein):**
    ```javascript
    import express from "express";
    const app = express();
    app.use(express.json());

    app.post(
    "/api/antrag/:antragId/dokument/:dokumentId/link",
    (req, res) => {
        console.log(
        `Mock linkDokument: antrag=${req.params.antragId}, dokument=${req.params.dokumentId}`
        );
        res.status(204).end();
    }
    );

    app.listen(5000, () => console.log("Antragsservice-Mock auf http://localhost:5000"));
    ```
2. **Mock-Service in neuem Terminal starten:**
    3. **Erwartung: Antragsservice-Mock auf http://localhost:5000“**
```bash
node src/mock-antrag.js
```

## 5. Datenbank initialisieren

1. **Ordner anlegen**
```bash
mkdir data
```
2. **Tabelle anlegen:**
```bash
node -e "import('./src/models/dokumentMetadaten.js').then(m=>m.initDb())"
```

## 6. Dokumentenservice Starten
1. **Im Rootverzeichnis:**
```bash
node src/index.js
```
## 7. Upload Testen

1. **In Git Bash**
```bash
curl -v \
  -F "antragId=test123" \
  -F "datei=@tests/fixtures/sample.pdf;type=application/pdf" \
  http://localhost:3000/api/dokumente
```
Ergebnis:
    HTTP/1.1 201 Created
    JSON-Antwort mit { dokumentID, status: "VALIDIERT", uploadDatum }
    Mock-Antragsterminal zeigt:
```php
Mock linkDokument: antrag=test123, dokument=<UUID>
```
    In MinIO-Bucket test-bucket erscheint sample.pdf


