#include <Arduino.h>

// Open Crypto Controll imports
#include "helper/helper.h"
#include "OCC/core/core.h"
#include "OCC/crypto/crypto.h"
#include "OCC/valise/valise.h"
#include "OCC/bip39/bip39.h"
#include "OCC/bip32/bip32.h"
#include "OCC/edsa/edsa.h"

#define BIP32_INITIAL_HARDENED_CHILD 0x80000000

// #include "sdkconfig.h" // generated by "make menuconfig"

#include "mbedtls/esp_config.h"

#define SDA2_PIN GPIO_NUM_18
#define SCL2_PIN GPIO_NUM_19

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

// Endianess
#define SPI_SHIFT_DATA(data, len) __builtin_bswap32((uint32_t)data << (32 - len))
#define SPI_REARRANGE_DATA(data, len) (__builtin_bswap32(data) >> (32 - len))

// forward declaration
void err(const char *message, void *data = NULL);

// HardwareSerial Serial0(0);
HWCDC SerialESP;
SLIPEncodedSerial SLIPSerial(SerialESP); // for XIAO ESP32C3
// SLIPEncodedSerial SLIPSerial(Serial); // for AI Thinker ESP-C3-32S

void setup()
{
    // begin communication via I²C
    // Wire.setPins(SDA_PIN, SCL_PIN);
    // Wire.begin(SDA_PIN, SCL_PIN);

    SLIPSerial.begin(115200);
    SerialESP.setRxBufferSize(1024);
    SerialESP.setTxBufferSize(1024);
}

void loop()
{
    OSCMessage msg;
    int size;
    // receive a bundle
    while (!SLIPSerial.endofPacket())
        if ((size = SLIPSerial.available()) > 0)
        {
            while (size--)
                msg.fill(SLIPSerial.read());
        }

    if (!msg.hasError())
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

        // check for inconsistency in libwally-core versions of code base
        msg.route("/IHW/wallyEcSigFromBytes", routeWallyEcSigFromBytes);
        msg.route("/IHW/wallyEcSigNormalize", routeWallyEcSigNormalize);
        msg.route("/IHW/wallyEcSigToDer", routeWallyEcSigToDer);
        msg.route("/IHW/wallyEcSigFromDer", routeWallyEcSigFromDer);
        msg.route("/IHW/wallyEcSigVerify2", routeWallyEcSigVerify2);
        msg.route("/IHW/wallyEcSigToPublicKey", routeWallyEcSigToPublicKey);
        msg.route("/IHW/wallyFormatBitcoinMessage", routeWallyFormatBitcoinMessage);
        msg.route("/IHW/wallyEcdh", routeWallyEcdh);

        // only available starting from version release_0.8.8
        // msg.route("/IHW/wallyS2cSigFromBytes", routeWallyS2cSigFromBytes);
        // msg.route("/IHW/wallyS2cCommitmentVerify", routeWallyS2cCommitmentVerify);

        /* Valise functions */
        msg.route("/IHW/valiseMnemonicSeedInit", routeValiseMnemonicSeedInit);
        msg.route("/IHW/valiseMnemonicSet", routeValiseMnemonicSet);
        msg.route("/IHW/valiseMnemonicGet", routeValiseMnemonicGet);
        msg.route("/IHW/valiseSeedSet", routeValiseSeedSet);
        msg.route("/IHW/valiseSeedGet", routeValiseSeedGet);
        msg.route("/IHW/valiseSign", routeValiseSign);

        /* Bip39 functions*/
        msg.route("/IHW/bip39GetLanguages", routeBip39GetLanguages);
        msg.route("/IHW/bip39GetWordlist", routeBip39GetWordlist);
        msg.route("/IHW/bip39GetWord", routeBip39GetWord);
        msg.route("/IHW/bip39Mnemonic", routeBip39Mnemonic);
        msg.route("/IHW/bip39MnemonicFromBytes", routeBip39MnemonicFromBytes);
        msg.route("/IHW/bip39MnemonicToBytes", routeBip39MnemonicToBytes);
        msg.route("/IHW/bip39MnemonicValidate", routeBip39MnemonicValidate);
        msg.route("/IHW/bip39MnemonicToSeed", routeBip39MnemonicToSeed);
        msg.route("/IHW/bip39MnemonicToSeed512", routeBip39MnemonicToSeed512);
        msg.route("/IHW/bip39MnemonicToPrivateKey", routeBip39MnemonicToPrivateKey);
        msg.route("/IHW/bip39NumberBouncer", routeBip39NumberBouncer);

        /* Bip32 functions*/
        msg.route("/IHW/bip32_key_init", routeBip32KeyInit);
        msg.route("/IHW/bip32_key_from_seed", routeBip32KeyFromSeed);
        msg.route("/IHW/bip32_key_from_parent", routeBip32KeyFromParent);
        msg.route("/IHW/bip32_key_from_parent2", routeBip32KeyFromParent2);
        msg.route("/IHW/bip32_key_from_parent_path_string", routeBip32KeyFromParentPathString);
        msg.route("/IHW/bip32_key_to_base58", routeBip32KeyToBase58);
        msg.route("/IHW/bip32_key_from_base58", routeBip32KeyFromBase58);
        msg.route("/IHW/bip32_key_serialize", routeBip32KeySerialize);
        msg.route("/IHW/bip32_key_unserialize", routeBip32KeyUnserialize);
        msg.route("/IHW/bip32_key_strip_private_key", routeBip32KeyStripPriateKey);
        msg.route("/IHW/bip32_key_get_fingerprint", routeBip32KeyGetFingerprint);

        /* ecdsa functions */
        msg.route("/IHW/ecdsaPubKey", routeEcdsaPubKey);
        msg.route("/IHW/ecdsaSigFromBytes", routeEcdsaSigFromBytes);
        msg.route("/IHW/ecdsaSigVerifyPubkeyHash", routeEcdsaSigVerifyPubkeyHash);
        msg.route("/IHW/ecdsaSigVerify", routeEcdsaSigVerify);

        /* Only available in most recent version 0.8.8 */
        msg.route("/IHW/wallySymKeyFromSeed", routeWallySymKeyFromSeed);
        msg.route("/IHW/wallySymKeyFromParent", routeWallySymKeyFromParent);

        msg.route("/IHW/trnd", routeTrnd);
        // msg.route("/IHW/valiseCbor", routeValiseCborEcho);
    }

    delay(20);
}
