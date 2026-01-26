#include "PapyrixWebServer.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FsHelpers.h>
#include <SDCardManager.h>
#include <WiFi.h>

#include "../config.h"
#include "html/FilesPageHtml.generated.h"
#include "html/HomePageHtml.generated.h"

namespace papyrix {

namespace {
// Static upload state (reused, not per-request heap)
FsFile uploadFile;
String uploadFileName;
String uploadPath = "/";
size_t uploadSize = 0;
bool uploadSuccess = false;
String uploadError = "";
}  // namespace

PapyrixWebServer::PapyrixWebServer() = default;

PapyrixWebServer::~PapyrixWebServer() { stop(); }

void PapyrixWebServer::begin() {
  if (running_) {
    Serial.println("[WEB] Server already running");
    return;
  }

  // Check network connection
  wifi_mode_t wifiMode = WiFi.getMode();
  bool isStaConnected = (wifiMode & WIFI_MODE_STA) && (WiFi.status() == WL_CONNECTED);
  bool isInApMode = (wifiMode & WIFI_MODE_AP);

  if (!isStaConnected && !isInApMode) {
    Serial.println("[WEB] Cannot start - no network connection");
    return;
  }

  apMode_ = isInApMode;

  Serial.printf("[WEB] Creating server on port %d (free heap: %d)\n", port_, ESP.getFreeHeap());

  server_.reset(new WebServer(port_));
  if (!server_) {
    Serial.println("[WEB] Failed to create WebServer");
    return;
  }

  // Setup routes
  server_->on("/", HTTP_GET, [this] { handleRoot(); });
  server_->on("/files", HTTP_GET, [this] { handleFileList(); });
  server_->on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server_->on("/api/files", HTTP_GET, [this] { handleFileListData(); });
  server_->on("/upload", HTTP_POST, [this] { handleUploadPost(); }, [this] { handleUpload(); });
  server_->on("/mkdir", HTTP_POST, [this] { handleCreateFolder(); });
  server_->on("/delete", HTTP_POST, [this] { handleDelete(); });
  server_->onNotFound([this] { handleNotFound(); });

  server_->begin();
  running_ = true;

  String ipAddr = apMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  Serial.printf("[WEB] Server started at http://%s/\n", ipAddr.c_str());
}

void PapyrixWebServer::stop() {
  if (!running_ || !server_) {
    return;
  }

  Serial.printf("[WEB] Stopping server (free heap: %d)\n", ESP.getFreeHeap());

  running_ = false;
  delay(100);

  server_->stop();
  delay(50);
  server_.reset();

  // Clear upload state
  if (uploadFile) {
    uploadFile.close();
  }
  uploadFileName = "";
  uploadPath = "/";
  uploadSize = 0;
  uploadSuccess = false;
  uploadError = "";

  Serial.printf("[WEB] Server stopped (free heap: %d)\n", ESP.getFreeHeap());
}

void PapyrixWebServer::handleClient() {
  if (!running_ || !server_) {
    return;
  }
  server_->handleClient();
}

void PapyrixWebServer::handleRoot() { server_->send(200, "text/html", HomePageHtml); }

void PapyrixWebServer::handleNotFound() { server_->send(404, "text/plain", "404 Not Found"); }

void PapyrixWebServer::handleStatus() {
  String ipAddr = apMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

  char json[256];
  snprintf(json, sizeof(json),
           "{\"version\":\"%s\",\"ip\":\"%s\",\"mode\":\"%s\",\"rssi\":%d,\"freeHeap\":%u,\"uptime\":%lu}",
           PAPYRIX_VERSION, ipAddr.c_str(), apMode_ ? "AP" : "STA", apMode_ ? 0 : WiFi.RSSI(), ESP.getFreeHeap(),
           millis() / 1000);

  server_->send(200, "application/json", json);
}

void PapyrixWebServer::handleFileList() { server_->send(200, "text/html", FilesPageHtml); }

void PapyrixWebServer::handleFileListData() {
  String currentPath = "/";
  if (server_->hasArg("path")) {
    currentPath = server_->arg("path");
    if (!currentPath.startsWith("/")) {
      currentPath = "/" + currentPath;
    }
    if (currentPath.length() > 1 && currentPath.endsWith("/")) {
      currentPath = currentPath.substring(0, currentPath.length() - 1);
    }
  }

  FsFile root = SdMan.open(currentPath.c_str());
  if (!root || !root.isDirectory()) {
    server_->send(404, "application/json", "[]");
    if (root) root.close();
    return;
  }

  server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_->send(200, "application/json", "");
  server_->sendContent("[");

  char name[256];
  bool seenFirst = false;
  FsFile file = root.openNextFile();

  while (file) {
    file.getName(name, sizeof(name));

    // Skip hidden items
    if (name[0] != '.' && !FsHelpers::isHiddenFsItem(name)) {
      JsonDocument doc;
      doc["name"] = name;
      doc["isDirectory"] = file.isDirectory();

      if (file.isDirectory()) {
        doc["size"] = 0;
        doc["isEpub"] = false;
      } else {
        doc["size"] = file.size();
        doc["isEpub"] = FsHelpers::isEpubFile(name);
      }

      char output[512];
      size_t written = serializeJson(doc, output, sizeof(output));
      if (written < sizeof(output)) {
        if (seenFirst) {
          server_->sendContent(",");
        } else {
          seenFirst = true;
        }
        server_->sendContent(output);
      }
    }

    file.close();
    file = root.openNextFile();
  }

  root.close();
  server_->sendContent("]");
  server_->sendContent("");
}

void PapyrixWebServer::handleUpload() {
  if (!running_ || !server_) return;

  HTTPUpload& upload = server_->upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadFileName = upload.filename;
    uploadSize = 0;
    uploadSuccess = false;
    uploadError = "";

    if (server_->hasArg("path")) {
      uploadPath = server_->arg("path");
      if (!uploadPath.startsWith("/")) {
        uploadPath = "/" + uploadPath;
      }
      if (uploadPath.length() > 1 && uploadPath.endsWith("/")) {
        uploadPath = uploadPath.substring(0, uploadPath.length() - 1);
      }
    } else {
      uploadPath = "/";
    }

    Serial.printf("[WEB] Upload start: %s to %s\n", uploadFileName.c_str(), uploadPath.c_str());

    String filePath = uploadPath;
    if (!filePath.endsWith("/")) filePath += "/";
    filePath += uploadFileName;

    if (SdMan.exists(filePath.c_str())) {
      SdMan.remove(filePath.c_str());
    }

    if (!SdMan.openFileForWrite("WEB", filePath, uploadFile)) {
      uploadError = "Failed to create file";
      Serial.printf("[WEB] Failed to create: %s\n", filePath.c_str());
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile && uploadError.isEmpty()) {
      size_t written = uploadFile.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        uploadError = "Write failed - disk full?";
        uploadFile.close();
      } else {
        uploadSize += written;
      }
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      if (uploadError.isEmpty()) {
        uploadSuccess = true;
        Serial.printf("[WEB] Upload complete: %s (%d bytes)\n", uploadFileName.c_str(), uploadSize);
      }
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) {
      uploadFile.close();
      String filePath = uploadPath;
      if (!filePath.endsWith("/")) filePath += "/";
      filePath += uploadFileName;
      SdMan.remove(filePath.c_str());
    }
    uploadError = "Upload aborted";
    Serial.println("[WEB] Upload aborted");
  }
}

void PapyrixWebServer::handleUploadPost() {
  if (uploadSuccess) {
    server_->send(200, "text/plain", "File uploaded: " + uploadFileName);
  } else {
    String error = uploadError.isEmpty() ? "Unknown error" : uploadError;
    server_->send(400, "text/plain", error);
  }
}

void PapyrixWebServer::handleCreateFolder() {
  if (!server_->hasArg("name")) {
    server_->send(400, "text/plain", "Missing folder name");
    return;
  }

  String folderName = server_->arg("name");
  if (folderName.isEmpty()) {
    server_->send(400, "text/plain", "Folder name cannot be empty");
    return;
  }

  String parentPath = "/";
  if (server_->hasArg("path")) {
    parentPath = server_->arg("path");
    if (!parentPath.startsWith("/")) {
      parentPath = "/" + parentPath;
    }
    if (parentPath.length() > 1 && parentPath.endsWith("/")) {
      parentPath = parentPath.substring(0, parentPath.length() - 1);
    }
  }

  String folderPath = parentPath;
  if (!folderPath.endsWith("/")) folderPath += "/";
  folderPath += folderName;

  if (SdMan.exists(folderPath.c_str())) {
    server_->send(400, "text/plain", "Folder already exists");
    return;
  }

  if (SdMan.mkdir(folderPath.c_str())) {
    Serial.printf("[WEB] Created folder: %s\n", folderPath.c_str());
    server_->send(200, "text/plain", "Folder created");
  } else {
    server_->send(500, "text/plain", "Failed to create folder");
  }
}

void PapyrixWebServer::handleDelete() {
  if (!server_->hasArg("path")) {
    server_->send(400, "text/plain", "Missing path");
    return;
  }

  String itemPath = server_->arg("path");
  String itemType = server_->hasArg("type") ? server_->arg("type") : "file";

  if (itemPath.isEmpty() || itemPath == "/") {
    server_->send(400, "text/plain", "Cannot delete root");
    return;
  }

  if (!itemPath.startsWith("/")) {
    itemPath = "/" + itemPath;
  }

  // Security: prevent deletion of hidden/system files
  String itemName = itemPath.substring(itemPath.lastIndexOf('/') + 1);
  if (itemName.startsWith(".") || FsHelpers::isHiddenFsItem(itemName.c_str())) {
    server_->send(403, "text/plain", "Cannot delete system files");
    return;
  }

  if (!SdMan.exists(itemPath.c_str())) {
    server_->send(404, "text/plain", "Item not found");
    return;
  }

  bool success = false;
  if (itemType == "folder") {
    FsFile dir = SdMan.open(itemPath.c_str());
    if (dir && dir.isDirectory()) {
      FsFile entry = dir.openNextFile();
      if (entry) {
        entry.close();
        dir.close();
        server_->send(400, "text/plain", "Folder not empty");
        return;
      }
      dir.close();
    }
    success = SdMan.rmdir(itemPath.c_str());
  } else {
    success = SdMan.remove(itemPath.c_str());
  }

  if (success) {
    Serial.printf("[WEB] Deleted: %s\n", itemPath.c_str());
    server_->send(200, "text/plain", "Deleted");
  } else {
    server_->send(500, "text/plain", "Failed to delete");
  }
}

}  // namespace papyrix
