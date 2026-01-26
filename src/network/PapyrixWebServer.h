#pragma once

#include <WebServer.h>

#include <memory>

namespace papyrix {

class PapyrixWebServer {
 public:
  PapyrixWebServer();
  ~PapyrixWebServer();

  void begin();
  void stop();
  void handleClient();

  bool isRunning() const { return running_; }
  uint16_t getPort() const { return port_; }

 private:
  std::unique_ptr<WebServer> server_;
  bool running_ = false;
  bool apMode_ = false;
  uint16_t port_ = 80;

  // Request handlers
  void handleRoot();
  void handleNotFound();
  void handleStatus();
  void handleFileList();
  void handleFileListData();
  void handleUpload();
  void handleUploadPost();
  void handleCreateFolder();
  void handleDelete();
};

}  // namespace papyrix
