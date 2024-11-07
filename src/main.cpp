#include <Arduino.h>
// Inisialisasi kebutuhan Library yang digunakan
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_camera.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <driver/rtc_io.h>
#include <time.h>
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// Menginisialisasi kebutuhan login Firebase
#define API_KEY "##########"
#define USER_EMAIL "##########"
#define USER_PASSWORD "##########"
#define STORAGE_BUCKET_ID "##########"
#define DATABASE_URL "##########"
#define FIREBASE_PROJECT_ID "##########"

// Inisialisasi path untuk menyimpan foto secara local
#define FILE_PHOTO "/photo.jpg"

// Inisialisasi pin modul kamera OV2640
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Penentapan variabel Library Firebase yang digunakan
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;
FirebaseJson json;

// Inisialisasi object dari library WiFiMulti
WiFiMulti wifiMulti;

// Inisialisasi kebutuhan variabel boolean
bool takeNewPhoto = false;
bool taskCompleted = true;
// Memastikan ukuran foto lebih dari 100 kb
bool checkPhoto(fs::FS &fs)
{
  File f_pic = fs.open(FILE_PHOTO);
  unsigned int pic_sz = f_pic.size();
  return (pic_sz > 100);
}

// Inisialisasi kebutuhan variabel integer
int timestamp;
int LED = 4;

// Inisialisasi kebutuhan variabel char
char ssid[] = "SATA Alin";
char password[] = "dasadarma@1973";
const char *ntpServer = "pool.ntp.org";

// Inisialisasi kebutuhan variabel String
String databasePath;
String parentPath;
String imagePath;
String outputPath;
String remoteImagePath;
String transferImagePath;
String uid;
String url;
String urlPath = "/imageUrl";
String timePath = "/timestamp";

// Simpan waktu millis sebelumnya
unsigned long sendDataPrevMillis = 0;

// Mendapatkan waktu timestamp
unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

// Function untuk inisialisasi WiFi
void initWiFi()
{
  delay(10);
  WiFi.mode(WIFI_STA);

  // Tambahkan list jaringan WiFi yang ingin disambungkan
  wifiMulti.addAP("SATA Alin", "dasadarma@1973");
  wifiMulti.addAP("Infinix 11s", "menorehpro");

  int n = WiFi.scanNetworks();
  Serial.println(F("scan done"));
  if (n == 0)
  {
    Serial.println(F("no networks found"));
  }
  else
  {
    Serial.print(n);
    Serial.println(F(" networks found"));
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }

  // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED)
  {
    Serial.println(F(""));
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());
    delay(1000);
  }
}

// Function untuk inisialisasi kamera
void initCamera()
{
  // Modul kamera OV2640
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Inisialisasi kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  // Konfigurasi filter pada kamera
  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, 0);                 // -2 to 2
  s->set_contrast(s, 0);                   // -2 to 2
  s->set_saturation(s, 0);                 // -2 to 2
  s->set_special_effect(s, 0);             // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);                   // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);                   // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);                    // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);              // 0 = disable , 1 = enable
  s->set_aec2(s, 0);                       // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);                   // -2 to 2
  s->set_aec_value(s, 300);                // 0 to 1200
  s->set_gain_ctrl(s, 1);                  // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);                   // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6
  s->set_bpc(s, 0);                        // 0 = disable , 1 = enable
  s->set_wpc(s, 1);                        // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);                    // 0 = disable , 1 = enable
  s->set_lenc(s, 1);                       // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);                    // 0 = disable , 1 = enable
  s->set_vflip(s, 0);                      // 0 = disable , 1 = enable
  s->set_dcw(s, 1);                        // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);                   // 0 = disable , 1 = enable
}

// Function inisialisasi LittleFS untuk penyimpanan foto secara local
void initLittleFS()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  }
  else
  {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }
}

void initFirebase()
{
  // Mengkonfigurasikan Firebase
  configF.database_url = DATABASE_URL;
  configF.api_key = API_KEY;
  // Melakukan login pada Firebase
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  configF.token_status_callback = tokenStatusCallback; // Lihat addons/TokenHelper.h
  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);

  // Mendapatkan user UID pada Firebase
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("");

  // Tampilkan user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);
}

// Function pengambilan foto dan disimpan pada LittleFS
void capturePhotoSaveLittleFS(void)
{
  // Buang gambar pertama karena kualitasnya buruk
  camera_fb_t *fb = NULL;
  // Lewati 3 frame pertama (tambah/kurangi jumlahnya sesuai kebutuhan)
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(LED, HIGH);
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
    digitalWrite(LED, LOW);
  }

  // Mengambil foto baru
  digitalWrite(LED, HIGH);
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    digitalWrite(LED, LOW);
    delay(1000);
    ESP.restart();
  }
  digitalWrite(LED, LOW);

  // Memberi nama pada foto
  Serial.printf("Picture file name: %s\n", FILE_PHOTO);
  File file = LittleFS.open(FILE_PHOTO, FILE_WRITE);

  // Memasukkan data pada file foto
  if (!file)
  {
    Serial.println("Failed to open file in writing mode");
  }
  else
  {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(FILE_PHOTO);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }
  // Tutup file
  file.close();
  esp_camera_fb_return(fb);
}

// Function untuk mengambil data perintah dari Firebase
void checkFirebaseData()
{
  // Memeriksa data dari Firebase setiap 1 detik
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    if (Firebase.RTDB.getInt(&fbdo, outputPath))
    {
      if (fbdo.dataType() == "int")
      {
        int readData = fbdo.intData();
        Serial.print("Data received: ");
        Serial.println(readData);

        // Jika menerima perintah 1, ubah variabel takeNewPhoto dan taskCompleted untuk dapat melakukan pengambilan foto
        if (readData == 1)
        {
          takeNewPhoto = true;
          taskCompleted = false;
          // Kembalikan data perintah ke 0
          if (Firebase.RTDB.setInt(&fbdo, outputPath, 0))
          {
            Serial.println("Change data to default Success");
          }
        }
      }
    }
    else
    {
      // Tampilkan error ketika Firebase error
      Serial.println(fbdo.errorReason());
    }
  }
}

// Funtion proses upload callback untuk Firebase Storage
void fcsUploadCallback(FCS_UploadStatusInfo info)
{
  if (info.status == firebase_fcs_upload_status_init)
  {
    Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  }
  else if (info.status == firebase_fcs_upload_status_upload)
  {
    Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  }
  else if (info.status == firebase_fcs_upload_status_complete)
  {
    Serial.println("Upload completed\n");
    FileMetaInfo meta = fbdo.metaData();
    Serial.printf("Name: %s\n", meta.name.c_str());
    Serial.printf("Bucket: %s\n", meta.bucket.c_str());
    Serial.printf("contentType: %s\n", meta.contentType.c_str());
    Serial.printf("Size: %d\n", meta.size);
    Serial.printf("Generation: %lu\n", meta.generation);
    Serial.printf("Metageneration: %lu\n", meta.metageneration);
    Serial.printf("ETag: %s\n", meta.etag.c_str());
    Serial.printf("CRC32: %s\n", meta.crc32.c_str());
    Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
    Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
  }
  else if (info.status == firebase_fcs_upload_status_error)
  {
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
  }
}

// Function untuk menjalankan proses upload foto ke Firebase
void uploadPicture()
{
  Serial.println("Uploading picture...");

  // Mendapatkan waktu timestamp sekarang
  timestamp = getTime();
  Serial.print("time: ");
  Serial.println(timestamp);

  // Mengatur path pada Firebase
  remoteImagePath = imagePath + "/" + String(timestamp) + ".jpg";
  transferImagePath = uid + "/latestImage";

  // Melakukan proses upload foto
  if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, FILE_PHOTO, mem_storage_type_flash, remoteImagePath, "image/jpeg", fcsUploadCallback))
  {
    Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());

    String urlLink = fbdo.downloadURL().c_str();

    // Mengatur path penyimpanan data pengambilan foto pada Firebase
    parentPath = databasePath + "/" + String(timestamp);

    // Menyimpan data pengambilan gambar pada Firebase
    json.set(urlPath.c_str(), urlLink);
    json.set(timePath.c_str(), timestamp);

    // Kirimkan URL dan timestamp foto ke Database
    Serial.printf("Update Live Image... %s\n", Firebase.RTDB.setJSON(&fbdo, transferImagePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Add tracker image... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  }
  else
  {
    // Tampilkan error ketika Firebase error
    Serial.println(fbdo.errorReason());
  }
}

void setup()
{
  Serial.begin(115200);

  // Melakukan inisialisasi seluruh yang dibutuhkan
  initWiFi();
  initLittleFS();
  initFirebase();
  // Mengkonfigurasikan waktu untuk ntp server
  configTime(0, 0, ntpServer);

  // Inisialisasi LED bawaan sebagai output
  pinMode(LED, OUTPUT);

  // Mematikan 'burnout detector' dan inisialisasi kamera
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  initCamera();

  // Mengatur variabel path
  databasePath = uid + "/data";
  imagePath = "/image/" + uid;
  outputPath = uid + "/door/takeImage";
}

void loop()
{
  // Memeriksa perintah data dari Firebase
  checkFirebaseData();

  // Jika variabel takeNewPhoto sama dengan true
  if (takeNewPhoto)
  {
    takeNewPhoto = false;
    capturePhotoSaveLittleFS();
  }

  // Jika Firebase siap dan taskCompleted sama dengan false
  if (Firebase.ready() && !taskCompleted)
  {
    taskCompleted = true;
    uploadPicture();
  }
}
