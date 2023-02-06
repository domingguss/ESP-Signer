
#ifndef GAuth_TCP_Client_H
#define GAuth_TCP_Client_H
#include <Arduino.h>
#include "ESP_Signer_Error.h"
#include "ESP_Signer_Const.h"
#include "mbfs/MB_FS.h"
#include "ESP_Signer_Helper.h"

#if defined(ESP32)
extern "C"
{
#include <esp_err.h>
#include <esp_wifi.h>
}

#include <WiFi.h>
#include <WiFiClient.h>
#include <ETH.h>
#include <WiFiClientSecure.h>
#if __has_include(<esp_idf_version.h>)
#include <esp_idf_version.h>
#endif

#elif defined(ESP8266)

#include <core_version.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#ifndef ARDUINO_ESP8266_GIT_VER
#error Your ESP8266 Arduino Core SDK is outdated, please update. From Arduino IDE go to Boards Manager and search 'esp8266' then select the latest version.
#endif

// 2.6.1 BearSSL bug
#if ARDUINO_ESP8266_GIT_VER == 0x482516e3
#error Due to bugs in BearSSL in ESP8266 Arduino Core SDK version 2.6.1, please update ESP8266 Arduino Core SDK to newer version. The issue was found here https:\/\/github.com/esp8266/Arduino/issues/6811.
#endif

#include <WiFiClientSecure.h>
#include <CertStoreBearSSL.h>
#define ESP_SIGNER_ESP_SSL_CLIENT BearSSL::WiFiClientSecure

#elif defined(PICO_RP2040)

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#endif

class GAuth_TCP_Client : public Client
{
  friend class GAuth_OAuth2_Client;
  friend class ESP_Signer;

public:
  GAuth_TCP_Client()
  {
#if defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
    client = nullptr;
#else
    client = std::unique_ptr<WiFiClientSecure>(new WiFiClientSecure());
#endif
  };
  virtual ~GAuth_TCP_Client() { release(); }

  void setCACert(const char *caCert)
  {

#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)

#if defined(ESP8266) || defined(PICO_RP2040)
    client->setBufferSizes(bsslRxSize, bsslTxSize);
#endif

    if (caCert != NULL)
    {
      certType = esp_signer_cert_type_data;
#if defined(ESP32)
      client->setCACert(caCert);
#elif defined(ESP8266) || defined(PICO_RP2040)
      x509 = new X509List(caCert);
      client->setTrustAnchors(x509);
#endif
    }
    else
    {
      certType = esp_signer_cert_type_none;
      client->stop();
#if defined(ESP32)
      client->setCACert(NULL);
#elif defined(ESP8266) || defined(PICO_RP2040)
      client->setNoDelay(true);
#endif
      setInsecure();
    }
#endif
  }

  bool setCertFile(const char *caCertFile, mb_fs_mem_storage_type storageType)
  {
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)

#if defined(ESP8266) || defined(PICO_RP2040)
    client->setBufferSizes(bsslRxSize, bsslTxSize);
#endif

    if (clockReady && strlen(caCertFile) > 0)
    {
      MB_String filename = caCertFile;
      if (filename.length() > 0)
      {
        if (filename[0] != '/')
          filename.prepend('/');
      }

      int len = mbfs->open(filename, storageType, mb_fs_open_mode_read);
      if (len > -1)
      {

#if defined(ESP32)

        if (storageType == mb_fs_mem_storage_type_flash)
        {
#if defined(MBFS_FLASH_FS)
          fs::File file = mbfs->getFlashFile();
          client->loadCACert(file, len);
          certType = esp_signer_cert_type_file;
#endif
          mbfs->close(storageType);
        }
        else if (storageType == mb_fs_mem_storage_type_sd)
        {

#if defined(MBFS_ESP32_SDFAT_ENABLED)

          if (cert)
            MemoryHelper::freeBuffer(mbfs, cert);

          cert = MemoryHelper::createBuffer<char *>(mbfs, len);
          if (mbfs->available(storageType))
            mbfs->read(storageType, (uint8_t *)cert, len);

          client->setCACert((const char *)cert);
          certType = esp_signer_cert_type_file;

#elif defined(MBFS_SD_FS)
          fs::File file = mbfs->getSDFile();
          client->loadCACert(file, len);
          certType = esp_signer_cert_type_file;
#endif
          mbfs->close(storageType);
        }

#elif defined(ESP8266) || defined(PICO_RP2040)
        uint8_t *der = MemoryHelper::createBuffer<uint8_t *>(mbfs, len);
        if (mbfs->available(storageType))
          mbfs->read(storageType, der, len);
        mbfs->close(storageType);
        client->setTrustAnchors(new X509List(der, len));
        MemoryHelper::freeBuffer(mbfs, der);
        certType = esp_signer_cert_type_file;
#endif
      }
    }

#endif

    return certType == esp_signer_cert_type_file;
  }

  void setInsecure()
  {
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
#if defined(ESP32)
#if __has_include(<esp_idf_version.h>)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(3, 3, 0)
    client->setInsecure();
#endif
#endif
#elif defined(ESP8266) || defined(PICO_RP2040)
    client->setInsecure();
#endif

#endif
  };

  void setBufferSizes(int rx, int tx)
  {
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
#if defined(ESP_8266) || defined(PICO_RP2040)
    bsslRxSize = rx;
    bsslTxSize = tx;
    if (client)
      client->setBufferSizes(rx, tx);
#endif
#endif
  }

  void ethDNSWorkAround(SPI_ETH_Module *eth, const char *host, uint16_t port)
  {
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)

    if (!eth)
      return;

#if defined(ESP8266) && defined(ESP8266_CORE_SDK_V3_X_X)

#if defined(INC_ENC28J60_LWIP)
    if (eth->enc28j60)
      goto ex;
#endif
#if defined(INC_W5100_LWIP)
    if (eth->w5100)
      goto ex;
#endif
#if defined(INC_W5500_LWIP)
    if (eth->w5500)
      goto ex;
#endif

#elif defined(PICO_RP2040)

#endif

    return;

#if defined(INC_ENC28J60_LWIP) || defined(INC_W5100_LWIP) || defined(INC_W5500_LWIP)
  ex:
    WiFiClient _client;
    _client.connect(host, port);
    _client.stop();
#endif

#endif
  }

  bool networkReady()
  {
#if defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)

    if (esp_signer_network_status_cb)
      esp_signer_network_status_cb();

    return esp_signer_networkStatus;
#else
    return WiFi.status() == WL_CONNECTED || ethLinkUp();
#endif
  }

  void networkReconnect()
  {
#if defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
    if (esp_signer_network_connection_cb)
      esp_signer_network_connection_cb();
#else
#if defined(ESP32)
    esp_wifi_connect();
#elif defined(ESP8266)
    WiFi.reconnect();
#endif
#endif
  }

  void networkDisconnect()
  {
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
    WiFi.disconnect();
#endif
  }

  int setTimeout(uint32_t timeoutmSec)
  {
    this->timeoutmSec = timeoutmSec;
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
#if defined(ESP32)
    return client->setTimeout(timeoutmSec / 1000);
#elif defined(ESP8266) || defined(PICO_RP2040)
    client->setTimeout(timeoutmSec);
#endif
#endif
    return 1;
  }

  bool begin(const char *host, uint16_t port, int *response_code)
  {
    this->host = host;
    this->port = port;
    this->response_code = response_code;
    return true;
  }

  operator bool()
  {
    return connected();
  }

  uint8_t connected()
  {
    if (!client)
      return 0;
    return client->connected();
  }

  bool connect()
  {
    if (connected())
    {
      flush();
      return true;
    }
    int ret = host.length() ? client->connect(host.c_str(), port) : client->connect(ip, port);
    if (!ret)
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_CONNECTION_REFUSED);

    client->setTimeout(timeoutmSec);

    return connected();
  }

  bool ethLinkUp()
  {
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
#if defined(ESP32)
    if (strcmp(ETH.localIP().toString().c_str(), (const char *)MBSTRING_FLASH_MCR("0.0.0.0")) != 0)
    {
      ETH.linkUp();
      return true;
    }
#elif defined(ESP8266) || defined(PICO_RP2040)
    if (!eth && config)
      eth = &(config->spi_ethernet_module);

    if (!eth)
      return false;

    bool ret = false;
#if defined(ESP8266) && defined(ESP8266_CORE_SDK_V3_X_X)

#if defined(INC_ENC28J60_LWIP)
    if (eth->enc28j60)
    {
      ret = eth->enc28j60->status() == WL_CONNECTED;
      goto ex;
    }
#endif
#if defined(INC_W5100_LWIP)
    if (eth->w5100)
    {
      ret = eth->w5100->status() == WL_CONNECTED;
      goto ex;
    }
#endif
#if defined(INC_W5500_LWIP)
    if (eth->w5500)
    {
      ret = eth->w5500->status() == WL_CONNECTED;
      goto ex;
    }
#endif

#elif defined(PICO_RP2040)

#endif

    return ret;

#if defined(INC_ENC28J60_LWIP) || defined(INC_W5100_LWIP) || defined(INC_W5500_LWIP)
  ex:
#endif

    // workaround for ESP8266 Ethernet
    delayMicroseconds(0);

    return ret;
#endif

#endif

    return false;
  }

  void release()
  {
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
    if (client)
    {
      client->stop();
      client.reset(nullptr);
      client.release();
      host.clear();

      if (cert)
        MemoryHelper::freeBuffer(mbfs, cert);

      certType = esp_signer_cert_type_undefined;
    }
#endif
  }

  int setError(int code)
  {
    if (!response_code)
      return -1000;

    *response_code = code;
    return *response_code;
  }

  void stop()
  {
    if (!client)
      return;

    return client->stop();
  };

  size_t write(const uint8_t *data, size_t size)
  {

    if (!data || !client)
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_SEND_REQUEST_FAILED);

    if (size == 0)
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_SEND_REQUEST_FAILED);

    if (!networkReady())
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_NOT_CONNECTED);

    if (!client->connected() && !connect())
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_CONNECTION_REFUSED);

    int res = client->write(data, size);

    if (res != (int)size)
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_SEND_REQUEST_FAILED);

    setError(ESP_SIGNER_ERROR_HTTP_CODE_OK);

    return size;
  }

  size_t write(uint8_t v)
  {
    uint8_t buf[1];
    buf[0] = v;
    return write(buf, 1);
  }

  int send(const char *data, int len = 0)
  {
    if (len == 0)
      len = strlen(data);
    return write((uint8_t *)data, len);
  }

  int print(const char *data)
  {
    return send(data);
  }

  int print(int data)
  {
    char *buf = MemoryHelper::createBuffer<char *>(mbfs, 64);
    sprintf(buf, (const char *)MBSTRING_FLASH_MCR("%d"), data);
    int ret = send(buf);
    MemoryHelper::freeBuffer(mbfs, buf);
    return ret;
  }

  int println(const char *data)
  {
    int len = send(data);
    if (len < 0)
      return len;
    int sz = send((const char *)MBSTRING_FLASH_MCR("\r\n"));
    if (sz < 0)
      return sz;
    return len + sz;
  }

  int println(int data)
  {
    char *buf = MemoryHelper::createBuffer<char *>(mbfs, 64);
    sprintf(buf, (const char *)MBSTRING_FLASH_MCR("%d\r\n"), data);
    int ret = send(buf);
    MemoryHelper::freeBuffer(mbfs, buf);
    return ret;
  }

  int available()
  {
    if (!client)
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_CONNECTION_REFUSED);

    return client->available();
  }

  int read(uint8_t *buf, size_t size)
  {
    return readBytes(buf, size);
  }

  int peek()
  {
    if (!client)
      return 0;
    return client->peek();
  }

  int read()
  {

    if (!client)
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_CONNECTION_REFUSED);

    int r = client->read();

    if (r < 0)
      return setError(ESP_SIGNER_ERROR_TCP_RESPONSE_READ_FAILED);

    return r;
  }

  int readBytes(uint8_t *buf, int len)
  {
    if (!client)
      return setError(ESP_SIGNER_ERROR_TCP_ERROR_CONNECTION_REFUSED);

    int r = client->readBytes(buf, len);

    if (r != len)
      return setError(ESP_SIGNER_ERROR_TCP_RESPONSE_READ_FAILED);

    setError(ESP_SIGNER_ERROR_HTTP_CODE_OK);

    return r;
  }

  int readBytes(char *buf, int len) { return readBytes((uint8_t *)buf, len); }

  void flush()
  {
    if (!client)
      return;

    while (client->available() > 0)
      client->read();
  }

  int connect(IPAddress ip, uint16_t port)
  {
    this->ip = ip;
    this->port = port;
    return connect();
  }
  int connect(const char *host, uint16_t port)
  {
    this->host = host;
    this->port = port;
    return connect();
  }

  esp_signer_cert_type getCertType()
  {
    return certType;
  }

  void setConfig(esp_signer_gauth_cfg_t *config, MB_FS *mbfs)
  {
    this->config = config;
    this->mbfs = mbfs;
  }

  void setClockStatus(bool status)
  {
    clockReady = status;
  }

#if defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
  void setClient(Client *client, ESP_Signer_NetworkConnectionRequestCallback networkConnectionCB,
                 ESP_Signer_NetworkStatusRequestCallback networkStatusCB)
  {

    this->client = client;
    this->esp_signer_network_connection_cb = networkConnectionCB;
    this->esp_signer_network_status_cb = networkStatusCB;
  }
#endif

  esp_signer_tcp_client_type type()
  {
#if defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
    return esp_signer_tcp_client_type_external;
#endif
    return esp_signer_tcp_client_type_internal;
  }

  bool isInitialized()
  {
#if defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
    return this->client != nullptr && esp_signer_network_status_cb != NULL && esp_signer_network_connection_cb != NULL;
#endif
    return true;
  }

  void setNetworkStatus(bool status)
  {
    esp_signer_networkStatus = status;
  }

private:
#if defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
  Client *client = nullptr;
#else
  std::unique_ptr<WiFiClientSecure> client = std::unique_ptr<WiFiClientSecure>(new WiFiClientSecure());
#endif
  ESP_Signer_NetworkConnectionRequestCallback esp_signer_network_connection_cb = NULL;
  ESP_Signer_NetworkStatusRequestCallback esp_signer_network_status_cb = NULL;
  volatile bool esp_signer_networkStatus = false;
  char *cert = NULL;
  esp_signer_cert_type certType = esp_signer_cert_type_undefined;
  MB_String host;
  uint16_t port = 0;
  IPAddress ip;
  int *response_code = nullptr;
  uint32_t timeoutmSec = 10000;
  esp_signer_gauth_cfg_t *config = nullptr;
  MB_FS *mbfs = nullptr;
#if !defined(ESP_SIGNER_ENABLE_EXTERNAL_CLIENT)
#if defined(ESP8266) || defined(PICO_RP2040)
#if defined(ESP8266)
  uint16_t bsslRxSize = 512;
  uint16_t bsslTxSize = 512;
#else
  uint16_t bsslRxSize = 16384;
  uint16_t bsslTxSize = 512;
#endif
  X509List *x509 = nullptr;
  SPI_ETH_Module *eth = NULL;
#endif
#endif

  bool clockReady = false;
};

#endif /* GAuth_TCP_Client_H */
