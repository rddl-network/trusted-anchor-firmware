#include <Arduino.h>
#include <Wire.h>
#include <HWCDC.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"


// #include "sdkconfig.h" // generated by "make menuconfig"

#include "mbedtls/esp_config.h"

#define SDA2_PIN GPIO_NUM_18
#define SCL2_PIN GPIO_NUM_19

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1


/* void i2c_master_init()
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA2_PIN,
		.scl_io_num = SCL2_PIN,
		.sda_pullup_en = GPIO_PULLUP_DISABLE,
		.scl_pullup_en = GPIO_PULLUP_DISABLE,
		//.master.clk_speed = 100000,
		};
			
	i2c_param_config(I2C_NUM_0 , &i2c_config);
	i2c_driver_install(I2C_NUM_0 , I2C_MODE_MASTER, 0, 0, 0);
}
*/

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <OSCBundle.h>
#include <OSCBoards.h>
#include <SLIPEncodedSerial.h>

//SLIPEncodedSerial SLIPSerial(Serial);  for regular ESP32
//HardwareSerial MySerial(0);
//SLIPEncodedSerial SLIPSerial(Serial); // for XIAO ESP32C3

#include "secp256k1.h"
#include "wally_core.h"
#include "wally_crypto.h"
#include "wally_bip32.h"
#include "wally_bip39.h"
#include "wally_address.h"

#include "wally_script.h"
#include "wally_psbt.h"

extern "C" {
#include "ccan/base64/base64.h"
}

// Endianess
#define SPI_SHIFT_DATA(data, len) __builtin_bswap32((uint32_t)data<<(32-len))
#define SPI_REARRANGE_DATA(data, len) (__builtin_bswap32(data)>>(32-len))

// forward declaration
void err(const char * message, void * data = NULL);

// root HD key
ext_key root;

#define FROMHEX_MAXLEN 512

void memzero(void *const pnt, const size_t len) {
/*#ifdef _WIN32
  SecureZeroMemory(pnt, len);
#elif defined(HAVE_MEMSET_S)
  memset_s(pnt, (rsize_t)len, 0, (rsize_t)len);
#elif defined(HAVE_EXPLICIT_BZERO)
  explicit_bzero(pnt, len);
#elif defined(HAVE_EXPLICIT_MEMSET)
  explicit_memset(pnt, 0, len);
#else*/
  volatile unsigned char *volatile pnt_ = (volatile unsigned char *volatile)pnt;
  size_t i = (size_t)0U;

  while (i < len) {
    pnt_[i++] = 0U;
  }
// #endif
}

const uint8_t *fromhex(const char *str) {
  static uint8_t buf[FROMHEX_MAXLEN];
  int len = strlen(str) / 2;
  if (len > FROMHEX_MAXLEN) len = FROMHEX_MAXLEN;
  for (int i = 0; i < len; i++) {
    uint8_t c = 0;
    if (str[i * 2] >= '0' && str[i * 2] <= '9') c += (str[i * 2] - '0') << 4;
    if ((str[i * 2] & ~0x20) >= 'A' && (str[i * 2] & ~0x20) <= 'F')
      c += (10 + (str[i * 2] & ~0x20) - 'A') << 4;
    if (str[i * 2 + 1] >= '0' && str[i * 2 + 1] <= '9')
      c += (str[i * 2 + 1] - '0');
    if ((str[i * 2 + 1] & ~0x20) >= 'A' && (str[i * 2 + 1] & ~0x20) <= 'F')
      c += (10 + (str[i * 2 + 1] & ~0x20) - 'A');
    buf[i] = c;
  }
  return buf;
}

void tohexprint(char *hexbuf, uint8_t *str, int strlen){
   // char hexbuf[strlen];
    for (int i = 0 ; i < strlen/2 ; i++) {
        sprintf(&hexbuf[2*i], "%02X", str[i]);
    }
  hexbuf[strlen-2] = '\0';
}

size_t toHex(const uint8_t * array, size_t arraySize, char * output, size_t outputSize){
    if(array == NULL || output == NULL){ return 0; }
    // uint8_t * array = (uint8_t *) arr;
    if(outputSize < 2*arraySize){
        return 0;
    }
    memzero(output, outputSize);

    for(size_t i=0; i < arraySize; i++){
        output[2*i] = (array[i] >> 4) + '0';
        if(output[2*i] > '9'){
            output[2*i] += 'a'-'9'-1;
        }

        output[2*i+1] = (array[i] & 0x0F) + '0';
        if(output[2*i+1] > '9'){
            output[2*i+1] += 'a'-'9'-1;
        }
    }
    return 2*arraySize;
}

String toHex(const uint8_t * array, size_t arraySize){
    if(array == NULL){ return String(); }
    size_t outputSize = arraySize * 2 + 1;
    char * output = (char *) malloc(outputSize);
    if(output == NULL){ return String(); }

    toHex(array, arraySize, output, outputSize);

    String result(output);

    memzero(output, outputSize);
    free(output);

    return result;
}

//HardwareSerial Serial0(0);
// HWCDC SerialESP;  // for XIAO ESP32C3
SLIPEncodedSerial SLIPSerial(Serial); // Serial for TA-1 SerialESP for seedstudio


void setup()
{
    //begin communication via I²C
    //Wire.setPins(SDA_PIN, SCL_PIN);
    //Wire.begin(SDA_PIN, SCL_PIN);

    SLIPSerial.begin(115200);
    Serial.begin(115200);
  
/*
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    //printf("\n");
    //printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");

        // Read
        printf("Reading restart counter from NVS ... ");
        int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
        switch (err) {
            case ESP_OK:
                printf("Done\n");
                printf("Restart counter = %d\n", restart_counter);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet!\n");
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
        // Write
        printf("Updating restart counter in NVS ... ");
        restart_counter++;
        err = nvs_set_i32(my_handle, "restart_counter", restart_counter);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        printf("Committing updates in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
        nvs_close(my_handle);
    }

    printf("\n");
  */
  /*
    // Restart module
    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
  */
    
    char mnemonic[] = "virtual venture head silk sing decline same online option route question powder color position bicycle feature inside hollow luggage dirt harvest mail leave jacket";
} 

void routeWallyInit(OSCMessage &msg, int addressOffset)
{
  wally_init(0x00);

  OSCMessage resp_msg("/wallyInit");
  resp_msg.add("0");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
}


void routeWallyCleanup(OSCMessage &msg, int addressOffset)
{
  wally_cleanup(0x00);

  OSCMessage resp_msg("/wallyCleanup");
  resp_msg.add("0");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
}

void routeWallyGetSecpContext(OSCMessage &msg, int addressOffset)
{
  secp256k1_context_struct *ctxStrct;
  wally_get_secp_context();

  OSCMessage resp_msg("/wallyGetSecpContext");
  resp_msg.add("0");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
}

void routeWallyGetNewSecpContext(OSCMessage &msg, int addressOffset)
{
  secp256k1_context_struct *ctxStrct;
  // wally_get_new_secp_context();

  OSCMessage resp_msg("/wallyGetNewSecpContext");
  resp_msg.add("0");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
}

void routeWallySecpContextFree(OSCMessage &msg, int addressOffset)
{
  secp256k1_context_struct *ctxStrct;
  //wally_secp_context_free(ctxStrct);

  OSCMessage resp_msg("/wallySecpContextFree");
  resp_msg.add("0");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
}


void routeWallyBZero(OSCMessage &msg, int addressOffset)
{
  char *bytes;
  size_t len;
  wally_bzero(bytes, len);

  OSCMessage resp_msg("/wallyBZero");
  resp_msg.add("0");
  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();
}

void routeWallyFreeString(OSCMessage &msg, int addressOffset)
{
  char *str;
  wally_free_string(str);

  OSCMessage resp_msg("/wallyBZero");
  resp_msg.add("0");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
}

void routeWallySecpRandomize(OSCMessage &msg, int addressOffset)
{
  unsigned char *bytes;
  size_t len;
  wally_secp_randomize(bytes, len);

  OSCMessage resp_msg("/wallyBZero");
  resp_msg.add("0");
  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();
}


/* ----------------------------------------------------------------*/
/* Crypto functions                                                */
/* ----------------------------------------------------------------*/

/* part of documentation but deprecated */
/* void routeWallyEcSigFromBytesLen(OSCMessage &msg, int addressOffset)
{
  int res;
  //uint8_t priv_key[33] = "7BC81198140916367B5CED9BADA28C37"; 
  uint8_t priv_key[33];
  size_t priv_key_len;
  // uint8_t hash_key[33] = "C99A85979AD295811330C5689C730250";
  uint8_t hash_key[33];
  size_t hash_key_len;
  uint32_t flags;
  size_t* len;

  priv_key_len = 32;
  hash_key_len = 32;
  int length;

  if(msg.isString(0))
  {
      length=msg.getDataLength(0);
      msg.getString(0, (char*)priv_key,length);
  }

  if(msg.isString(2))
  {
      length=msg.getDataLength(2);
      msg.getString(2, (char*)hash_key,length);  
  }


  res = wally_ec_sig_from_bytes_len(
            (uint8_t*)priv_key, 
            priv_key_len, 
            (uint8_t*)hash_key, 
            hash_key_len, 
            EC_FLAG_ECDSA, 
            (unsigned char*)len
        );

  OSCMessage resp_msg("/IHW/wallyEcSigFromBytesLen");
  resp_msg.add(len);

  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();
} */

void routeWallyEcSigFromBytes(OSCMessage &msg, int addressOffset)
{
  int res;

  uint8_t priv_key[32];
  char char_priv_key[65]; // has to be inside the msg.isString() check ...

  uint8_t hash_key[32];
  char char_hash_key[65]; // has to be inside the msg.isString() check ...

  uint8_t bytes_out[64];


  if(msg.isString(0))
  {
      int length=msg.getDataLength(0);
      msg.getString(0, char_priv_key, length);
  }
  if(msg.isString(2))
  {
      int length=msg.getDataLength(2);
      msg.getString(2, char_hash_key, length); 
  }

  
  memcpy(priv_key,
         fromhex((const char*)char_priv_key),
         32
  );
  memcpy(hash_key,
         fromhex((const char*)char_hash_key),
         32
  );

  res = wally_ec_sig_from_bytes(
            priv_key,
            32,
            hash_key,
            32,
            EC_FLAG_ECDSA, 
            bytes_out, 
            64
  );

  /* Requirement by Arduino to stream strings back to requestor */
  String hexStr;
  hexStr = toHex(bytes_out, 64);

  OSCMessage resp_msg("/IHW/wallyEcSigFromBytes");
  resp_msg.add(hexStr.c_str());
  resp_msg.add(char_priv_key);
  resp_msg.add(char_hash_key);


  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();

  wally_free_string(char_priv_key);
  wally_free_string(char_hash_key);

}

void routeWallyEcSigNormalize(OSCMessage &msg, int addressOffset)
{
  int res;

  uint8_t sig[64];
  char char_sig[129];

  uint8_t bytes_out[64]; 
 

  if(msg.isString(0))
  {
      int length=msg.getDataLength(0);
      msg.getString(0, char_sig, length);
  }

  memcpy(sig,
         fromhex(char_sig),
         64
  );

  res = wally_ec_sig_normalize(
            sig, 
            EC_SIGNATURE_LEN, 
            bytes_out, 
            EC_SIGNATURE_LEN
  );

  /* Requirement by Arduino to stream strings back to requestor */
  String hexStr;
  hexStr = toHex(bytes_out, 64);

  OSCMessage resp_msg("/IHW/wallyEcSigNormalize");
  resp_msg.add(hexStr.c_str());
  // resp_msg.add("test message");
  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();
}

void routeWallyEcSigToDer(OSCMessage &msg, int addressOffset)
{
  int res;

  uint8_t sig[64];

  char str_sig[129];

  uint8_t der[73]; 

  size_t len;

  if(msg.isString(0))
  {
      int length=msg.getDataLength(0);
      msg.getString(0, str_sig,length);
  }

  memcpy(sig,
         fromhex(str_sig),
         64
  );

  res = wally_ec_sig_to_der(
            sig, 
            EC_SIGNATURE_LEN, 
            der, 
            EC_SIGNATURE_DER_MAX_LEN,
            &len
  );

  /* Requirement by Arduino to stream strings back to requestor */
  String hexStr;
  hexStr = toHex(der, EC_SIGNATURE_DER_MAX_LEN);

  OSCMessage resp_msg("/IHW/wallyEcSigToDer");
  resp_msg.add(hexStr.c_str());
  //resp_msg.add("test message");
  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();
}

void routeWallyEcSigFromDer(OSCMessage &msg, int addressOffset)
{
  int res;

  uint8_t der[72];

  char str_der[145];

  uint8_t sig[64]; 
 

  if(msg.isString(0))
  {
    int length=msg.getDataLength(0);
    msg.getString(0, (char*)str_der, length);
  }
  memcpy(der, 
        fromhex(str_der),
         72
  );

  res = wally_ec_sig_from_der(
            der, 
            72, 
            sig, 
            64
  );

  /* Requirement by Arduino to stream strings back to requestor */
  String hexStr;
  hexStr = toHex(sig, 64);

  OSCMessage resp_msg("/IHW/wallyEcSigFromDer");
  resp_msg.add(hexStr.c_str());
  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();
}

void routeWallyEcSigVerify(OSCMessage &msg, int addressOffset)
{
  int res;
  uint8_t bytes_der[72];
  uint8_t bytes_out[64]; 
  int bytes_der_len; 
  int len;
}


/* ----------------------------------------------------------------*/
/* wally bip39 functions                                           */
/* ----------------------------------------------------------------*/

void routeBip39GetLanguages(OSCMessage &msg, int addressOffset)
{
  int res;
  char *output = NULL;

  res = bip39_get_languages(&output);

  OSCMessage resp_msg("/bip39GetLanguages");
  resp_msg.add(output);
  SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
  SLIPSerial.endPacket(); 
  resp_msg.empty();

  wally_free_string(output);
}

void routeBip39GetWordlist(OSCMessage &msg, int addressOffset)
{
  int res;

  if(msg.isString(0))
  {
    struct words *output;
    int length=msg.getDataLength(0);
    char lang[length];
    msg.getString(0,lang,length);

    res = bip39_get_wordlist(lang, &output);

    OSCMessage resp_msg("/bip39GetWordlist");
    resp_msg.add(output);
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
  }
}

void routeBip39GetWord(OSCMessage &msg, int addressOffset)
{
  int res;
  struct words *w;

  if(msg.isString(0))
  {
    int length=msg.getDataLength(0);
    char lang[length];
    msg.getString(0,lang,length);
    res = bip39_get_wordlist(lang, &w);
  }

  if(msg.isInt(1))
  {
    // don't forget problem with endianess
    char *output;
    int nth_word = msg.getInt(1);
    res = bip39_get_word(w, nth_word, &output);

    OSCMessage resp_msg("/bip39GetWordlist");

    char str_nth_word[10];
    sprintf(str_nth_word, "%d", nth_word);
    resp_msg.add(str_nth_word);
    resp_msg.add(output);

    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();

    wally_free_string(output);
  }
}

void routeBip39NumberBouncer(OSCMessage &msg, int addressOffset)
{
  int res;

  if(msg.isInt(0))
  {
    int nth_word = msg.getInt(0);

    OSCMessage resp_msg("/bip39GetNumberBouncer");

    char str_nth_word[10];
    sprintf(str_nth_word, "%d", nth_word);
    resp_msg.add((int32_t)nth_word);

    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
      resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
  }
}

void routeBipMnemonicFromBytes(OSCMessage &msg, int addressOffset)
{
  boolean b;
}

void routeBipMnemonicToBytes(OSCMessage &msg, int addressOffset)
{
  boolean b;
}

void routeBipMnemonicValidate(OSCMessage &msg, int addressOffset)
{
  boolean b;
}

void routeBipMnemonicToSeed(OSCMessage &msg, int addressOffset)
{
  boolean b;
}

void routeBipMnemonicToSeed512(OSCMessage &msg, int addressOffset)
{
  boolean b;
}


void routeMnemonic(OSCMessage &msg, int addressOffset)
{
    int res;
    size_t len;
 
    uint8_t se_rnd[32] = {0};
    esp_fill_random(se_rnd, 32);

    char *phrase = NULL;
    res = bip39_mnemonic_from_bytes(NULL, se_rnd, sizeof(se_rnd), &phrase);

    msg.add(phrase);
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    msg.empty();
}

void routeEntropy(OSCMessage &msg, int addressOffset)
{
    int res;
    size_t len;

    uint8_t se_rnd[32] = {0};
    esp_fill_random(se_rnd, 32);

    //const char *seed;
    //seed = toHex(se_rnd, 32).c_str();

    char seed[32];
  
    memset(seed, '\0', sizeof(seed));
    for (int i=0; i<31; i++)
    {
        static char tmp[4] = {};
        sprintf(tmp, "%02X", se_rnd[i]);
        strcpy(seed+i, tmp);
    }

    //msg.add("2573548DF4251F3048ABA137EFEEC11E59C0738D47C88B46462EDE80BEFFA2CA");

    msg.add(seed);
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    msg.empty();
}

void routeBip32KeyFromSeed(OSCMessage &msg, int addressOffset)
{
    int res;
    int res2;
    size_t len;

    if(msg.isString(0))
    {
        //res = wally_init(0);

        int length=msg.getDataLength(0);

        char hexStr[length];
        msg.getString(0,hexStr,length);

        /**************** BIP-39 recovery phrase ******************/

        // random buffer should be generated by TRNG or somehow else
        // but we will use predefined one for demo purposes
        // 16 bytes will generate a 12-word recovery phrase
        uint8_t rnd[] = {
            0xAC, 0x91, 0xD3, 0xBC, 0x1B, 0x7C, 0x06, 0x2E,
            0x21, 0xB5, 0x86, 0xA0, 0x2D, 0xBE, 0x5D, 0x24
        };

         // creating a recovery phrase
        char *phrase = NULL;
        res = bip39_mnemonic_from_bytes(NULL, (const unsigned char*)fromhex((const char*)hexStr), 32, &phrase);
        //res = bip39_mnemonic_from_bytes(NULL, rnd, sizeof(rnd), &phrase);
        

        // converting recovery phrase to seed
        uint8_t seed[BIP39_SEED_LEN_512];
        res2 = bip39_mnemonic_to_seed(phrase, "my password", seed, sizeof(seed), &len);

        // don't forget to securely clean up the string when done
        wally_free_string(phrase);

        res = bip32_key_from_seed(seed, sizeof(seed), BIP32_VER_TEST_PRIVATE, 0, &root);
        //get base58 xprv string
        char *xprv = NULL;
        res = bip32_key_to_base58(&root, BIP32_FLAG_KEY_PRIVATE, &xprv);

        msg.add(xprv);
    }

    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    msg.empty();
}

void routeTrnd(OSCMessage &msg, int addressOffset)
{
    int len;

    if (msg.isInt(0))
    { 
        len = msg.getInt(0);
        uint8_t se_rnd[len] = {0};
        esp_fill_random(se_rnd, len);

        char trnd[len];
        memset(trnd, '\0', sizeof(trnd));
        for (int i=0; i<len-1; i++)
        {
            static char tmp[4] = {};
            sprintf(tmp, "%02X", se_rnd[i]);
            strcpy(trnd+i, tmp);
        }

        msg.add(trnd);
        SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
            msg.send(SLIPSerial);
        SLIPSerial.endPacket(); 
        msg.empty();
    }
}

void routeCborEcho(OSCMessage &msg, int addressOffset)
{

    OSCMessage msg2("/cbor/echo");
    int res;
    size_t len;

    uint8_t se_rnd[32] = {0};
    esp_fill_random(se_rnd, 32);

    char *phrase = NULL;
    res = bip39_mnemonic_from_bytes(NULL, se_rnd, sizeof(se_rnd), &phrase);

    msg2.add("d08355a20101055001010101010101010101010101010101a10458246d65726961646f632e6272616e64796275636b406275636b6c616e642e6578616d706c655820c4af85ac4a5134931993ec0a1863a6e8c66ef4c9ac16315ee6fecd9b2e1c79a1");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        msg2.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    msg.empty();
}

void routeMnemonicToBytes(OSCMessage &msg, int addressOffset)
{
    int res;
    size_t len;
    uint8_t bytes_out[BIP39_SEED_LEN_512];

    if(msg.isString(0))
    {
      int length = msg.getDataLength(0);
      char phrase[length];
      msg.getString(0,phrase,length);
      //Serial.println(phrase);

      // converting recovery phrase to bytes
      res = bip39_mnemonic_to_bytes(NULL, phrase, bytes_out, sizeof(bytes_out), &len);
    }

    String hexStr;
    hexStr = toHex(bytes_out, 32);
    //Serial.println(hexStr);

    OSCMessage resp_msg("/mnemonicToBytes");
    resp_msg.add(hexStr.c_str());
    //resp_msg.add("hello");
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
    delay(20);
}

void routeMnemonicFromBytes(OSCMessage &msg, int addressOffset)
{
    int res;
    size_t len;
    uint8_t bytes_out[BIP39_SEED_LEN_512];
    char *phrase = NULL;

    if(msg.isString(0))
    {
      int length = msg.getDataLength(0);
      char hexStr[length];
      msg.getString(0,hexStr,length);
      //Serial.println(hexStr);
      
      res = bip39_mnemonic_from_bytes(NULL, (const unsigned char*)fromhex(hexStr), 32, &phrase);
    }

    OSCMessage resp_msg("/mnemonicFromBytes");
    resp_msg.add(phrase);
    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        resp_msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    resp_msg.empty();
    delay(20);
}

void routeBip32KeyFromParent(OSCMessage &msg, int addressOffset)
{
    int res;
    int res2;
    size_t len;

    if(msg.isString(0))
    {
        //res = wally_init(0);

        int length=msg.getDataLength(0);

        char hexStr[length];
        msg.getString(0,hexStr,length);

        /**************** BIP-39 recovery phrase ******************/

        // random buffer should be generated by TRNG or somehow else
        // but we will use predefined one for demo purposes
        // 16 bytes will generate a 12-word recovery phrase
        uint8_t rnd[] = {
            0xAC, 0x91, 0xD3, 0xBC, 0x1B, 0x7C, 0x06, 0x2E,
            0x21, 0xB5, 0x86, 0xA0, 0x2D, 0xBE, 0x5D, 0x24
        };

         // creating a recovery phrase
        char *phrase = NULL;
        res = bip39_mnemonic_from_bytes(NULL, (const unsigned char*)fromhex((const char*)hexStr), 32, &phrase);
        //res = bip39_mnemonic_from_bytes(NULL, rnd, sizeof(rnd), &phrase);
        

        // converting recovery phrase to seed
        uint8_t seed[BIP39_SEED_LEN_512];
        res2 = bip39_mnemonic_to_seed(phrase, "my password", seed, sizeof(seed), &len);

        // don't forget to securely clean up the string when done
        wally_free_string(phrase);

        res = bip32_key_from_seed(seed, sizeof(seed), BIP32_VER_TEST_PRIVATE, 0, &root);
        // get base58 xprv string
        // char *xprv = NULL;
        // res = bip32_key_to_base58(&root, BIP32_FLAG_KEY_PRIVATE, &xprv);

        // master = ext_key ()
        // ret = bip32_key_from_parent_path_str_n(master, 'm/0h/0h/'+str(x)+'h', len('m/0h/0h/'+str(x)+'h'), 0, FLAG_KEY_PRIVATE, derive_key_out)
  
        // ('bip32_key_from_parent_path_str', c_int, [POINTER(ext_key), c_char_p, c_uint32, c_uint32, POINTER(ext_key)]),

        // deriving account key for native segwit, testnet: m/84h/1h/0h
        ext_key pk;
        uint32_t path[] = {
          BIP32_INITIAL_HARDENED_CHILD+84, // 84h
          BIP32_INITIAL_HARDENED_CHILD+1,  // 1h
          BIP32_INITIAL_HARDENED_CHILD     // 0h
        };

        res = bip32_key_from_parent_path(&root,path, 3, BIP32_FLAG_KEY_PRIVATE, &pk);

        char *xprv = NULL;
        res = bip32_key_to_base58(&pk, BIP32_FLAG_KEY_PRIVATE, &xprv);
        msg.add(xprv);
    }

    SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
        msg.send(SLIPSerial);
    SLIPSerial.endPacket(); 
    msg.empty();
}

void routeBip32KeyFromParentPath(OSCMessage &msg, int addressOffset)
{
  boolean b;
}

void routeBip32KeyFromBase58(OSCMessage &msg, int addressOffset)
{
  boolean b;
}

void routeSlip21KeyFromSeed(OSCMessage &msg, int addressOffset)
{
  boolean b;
}

void loop()
{
    OSCMessage msg;
    int size;
    //receive a bundle
    while(!SLIPSerial.endofPacket())
        if( (size =SLIPSerial.available()) > 0)
        {
            while(size--)
              msg.fill(SLIPSerial.read());
        }
    
    if(!msg.hasError())
    {
      /* Core functions */
        msg.route("/IHW/wallyInit", routeWallyInit);
        msg.route("/IHW/wallyCleanup", routeWallyCleanup);
        msg.route("/IHW/wallyGetSecpContext", routeWallyGetSecpContext);
        msg.route("/IHW/wallyGetNewSecpContext", routeWallyGetNewSecpContext);
        msg.route("/IHW/wallySecpContextFree", routeWallySecpContextFree);
        msg.route("/IHW/wallyBZero", routeWallyBZero);
        msg.route("/IHW/wallyFreeString", routeWallyFreeString);
        msg.route("/IHW/wallySecpRandomize", routeWallySecpRandomize);

      /* Crypto functions */
        
        // deprecated in libwally-core code base
        // msg.route("/IHW/wallyEcSigFromBytesLen", routeWallyEcSigFromBytesLen);
        msg.route("/IHW/wallyEcSigFromBytes", routeWallyEcSigFromBytes);
        msg.route("/IHW/wallyEcSigNormalize", routeWallyEcSigNormalize);
        msg.route("/IHW/wallyEcSigToDer", routeWallyEcSigToDer);
        msg.route("/IHW/wallyEcSigFromDer", routeWallyEcSigFromDer);

      /* Bip39 functions*/
        msg.route("/IHW/bip39GetLanguages", routeBip39GetLanguages);
        msg.route("/IHW/bip39GetWordlist", routeBip39GetWordlist);
        msg.route("/IHW/bip39GetWord", routeBip39GetWord);
        msg.route("/IHW/bip39MnemonicFromBytes", routeBipMnemonicFromBytes);
        msg.route("/IHW/bip39MnemonicToBytes", routeBipMnemonicToBytes);
        msg.route("/IHW/bip39MnemonicValidate", routeBipMnemonicValidate);
        msg.route("/IHW/bip39MnemonicToSeed", routeBipMnemonicToSeed);
        msg.route("/IHW/bip39MnemonicToSeed512", routeBipMnemonicToSeed512);
        msg.route("/IHW/bip39NumberBouncer", routeBip39NumberBouncer);

        msg.route("/IHW/mnemonic", routeMnemonic);
        msg.route("/IHW/mnemonicFromBytes", routeMnemonicFromBytes);
        msg.route("/IHW/mnemonicToBytes", routeMnemonicToBytes);
        //msg.route("/IHW/seed", routeEntropy);
        msg.route("/IHW/bip32_key_from_seed", routeBip32KeyFromSeed);
        msg.route("/IHW/bip32_key_from_parent", routeBip32KeyFromParent);
        // msg.route("/IHW/bip32_key_from_parent_path", routeBip32KeyFromParentPath);
        // msg.route("/IHW/bip32_key_from_base58", routeBip32KeyFromBase58);
        msg.route("/IHW/slip21_key_from_seed", routeSlip21KeyFromSeed);
        msg.route("/IHW/trnd", routeTrnd);
        // msg.route("/IHW/cbor", routeCborEcho);
    }

    /*if(!msg.hasError())
    {
        static int32_t sequencenumber=0;
        // we can sneak an addition onto the end of the bundle
        msg.add("/sequencenumber").add(sequencenumber++);
        SLIPSerial.beginPacket(); // mark the beginning of the OSC Packet
            msg.send(SLIPSerial);
        SLIPSerial.endPacket();     
    }*/


  delay(20);
}
