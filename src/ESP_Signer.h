/**
 * Google's OAuth2.0 Access token Generation class, Signer.h version 1.2.1
 *
 * This library used RS256 for signing algorithm.
 *
 * The signed JWT token will be generated and exchanged with the access token in the final generating process.
 *
 * This library supports Espressif ESP8266 and ESP32
 *
 * Created November 17, 2022
 *
 * The MIT License (MIT)
 * Copyright (c) 2022 K. Suwatchai (Mobizt)
 *
 *
 * Permission is hereby granted, free of charge, to any person returning a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ESP_SIGNER_H
#define ESP_SIGNER_H

#include <Arduino.h>
#include "ESP_Signer_Utils.h"

class ESP_Signer
{

public:
    ESP_Signer();
    ~ESP_Signer();

    /**
     * Begin the Access token generation
     *
     * @param config The pointer to SignerConfig structured data contains the authentication credentials
     *
     */
    void begin(SignerConfig *config);

    /**
     * End the Access token generation
     *
     */
    void end();

    /**
     * Check the token ready state and trying to re-generate the token when expired.
     *
     * @return Boolean of ready state.
     *
     */
    bool tokenReady();

    /**
     * Get the generated access token.
     *
     * @return String of OAuth2.0 access token.
     *
     */
    String accessToken();

    /**
     * Get the token type string.
     *
     * @param info The TokenInfo structured data contains token info.
     * @return token type String.
     *
     */
    String getTokenType();
    String getTokenType(TokenInfo info);

    /**
     * Get the token status string.
     *
     * @param info The TokenInfo structured data contains token info.
     * @return token status String.
     *
     */
    String getTokenStatus();
    String getTokenStatus(TokenInfo info);

    /**
     * Get the token generation error string.
     *
     * @param info The TokenInfo structured data contains token info.
     * @return token generation error String.
     *
     */
    String getTokenError();
    String getTokenError(TokenInfo info);

    /**
     * Get the token expiration timestamp (seconds from midnight Jan 1, 1970).
     *
     * @return timestamp.
     *
     */
    unsigned long getExpiredTimestamp();

    /**
     * Refresh the access token
     *
     */
    void refreshToken();

    /** Set system time with timestamp.
     *
     * @param ts timestamp in seconds from midnight Jan 1, 1970.
     * @return Boolean type status indicates the success of the operation.
     */
    bool setSystemTime(time_t ts);

#if defined(MBFS_SD_FS) && defined(MBFS_CARD_TYPE_SD)

    /** SD card config with GPIO pins.
     *
     * @param ss SPI Chip/Slave Select pin.
     * @param sck SPI Clock pin.
     * @param miso SPI MISO pin.
     * @param mosi SPI MOSI pin.
     * @return Boolean type status indicates the success of the operation.
     */
    bool sdBegin(int8_t ss = -1, int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1);

#if defined(ESP8266)

    /** SD card config with SD FS configurations (ESP8266 only).
     *
     * @param ss SPI Chip/Slave Select pin.
     * @param sdFSConfig The pointer to SDFSConfig object (ESP8266 only).
     * @return Boolean type status indicates the success of the operation.
     */
    bool sdBegin(SDFSConfig *sdFSConfig);

#endif

#if defined(ESP32)
    /** SD card config with chip select and SPI configuration (ESP32 only).
     *
     * @param ss SPI Chip/Slave Select pin.
     * @param spiConfig The pointer to SPIClass object for SPI configuartion (ESP32 only).
     * @return Boolean type status indicates the success of the operation.
     */
    bool sdBegin(int8_t ss, SPIClass *spiConfig = nullptr);
#endif

#if defined(MBFS_ESP32_SDFAT_ENABLED) || defined(MBFS_SDFAT_ENABLED)
    /** SD card config with SdFat SPI and pins configurations (ESP32 with SdFat included only).
     *
     * @param sdFatSPIConfig The pointer to SdSpiConfig object for SdFat SPI configuration.
     * @param ss SPI Chip/Slave Select pin.
     * @param sck SPI Clock pin.
     * @param miso SPI MISO pin.
     * @param mosi SPI MOSI pin.
     * @return Boolean type status indicates the success of the operation.
     */
    bool sdBegin(SdSpiConfig *sdFatSPIConfig, int8_t ss = -1, int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1);
#endif

#endif

#if defined(ESP32) && defined(MBFS_SD_FS) && defined(MBFS_CARD_TYPE_SD_MMC)
    /** Initialize the SD_MMC card (ESP32 only).
     *
     * @param mountpoint The mounting point.
     * @param mode1bit Allow 1 bit data line (SPI mode).
     * @param format_if_mount_failed Format SD_MMC card if mount failed.
     * @return The boolean value indicates the success of operation.
     */
    bool sdMMCBegin(const char *mountpoint = "/sdcard", bool mode1bit = false, bool format_if_mount_failed = false);
#endif

protected:
    SignerUtils *ut = nullptr;
    MB_FS *mbfs = nullptr;
    SignerConfig *config = nullptr;
    esp_signer_callback_function_t _cb = nullptr;
    struct token_info_t tokenInfo;

    bool authenticated = false;
    bool _token_processing_task_enable = false;
    bool _token_processing_task_end_request = false;

    unsigned long unauthen_millis = 0;
    unsigned long unauthen_pause_duration = 3000;

    bool tokenSAReady();
    bool parseSAFile();
    bool handleToken();
    void clearSA();
    void setTokenError(int code);
    bool handleSignerError(int code, int httpCode = 0);
    bool parseJsonResponse(PGM_P key_path);
    bool handleTokenResponse(int &httpCode);
    void tokenProcessingTask();
    bool createJWT();
    bool requestTokens();
    void getExpiration(const char *exp);
    void checkToken();
    void errorToString(int httpCode, MB_String &buff);
    void sendTokenStatusCB();
    unsigned long getExpireMS();
    bool isExpired();
    SignerConfig *getCfg();
    bool reconnect(unsigned long dataTime);

#if defined(ESP8266)
    void set_scheduled_callback(esp_signer_callback_function_t callback)
    {
        _cb = std::move([callback]()
                        { schedule_function(callback); });
        _cb();
    }
#endif
};

extern ESP_Signer Signer;

#endif