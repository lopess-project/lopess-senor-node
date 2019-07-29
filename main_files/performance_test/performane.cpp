extern "C" {
  #include "hss.h"
  #include "ed25519.h"
  #include "mbedtls/sha256.h"
  #include "uECC_vli.h"
  #include "uECC.h"
  #include "params.h"
  #include "xmss.h"
  #include "randombytes.h"
  #include "bliss_b_errors.h"
  #include "bliss_b_keys.h"
  #include "bliss_b_signatures.h"
  #include "entropy.h"
  #include "bliss_b_utils.h"
}
#include <stdio.h>
#include <stdarg.h>
#include <types.h>
#include <Arduino.h>

#define DEFAULT_AUX_DATA 8192
#define XMSS_MLEN 64

#ifndef XMSS_SIGNATURES
    #define XMSS_SIGNATURES 2
#endif

#define XMSS_PARSE_OID xmss_parse_oid
#define XMSS_STR_TO_OID xmss_str_to_oid
#define XMSS_KEYPAIR xmss_keypair
#define XMSS_SIGN xmss_sign
#define XMSS_SIGN_OPEN xmss_sign_open
#define XMSS_VARIANT "XMSS-SHA2_10_256"

unsigned long startMillis;
unsigned long currentMillis;

extern "C" {
    void app_main(void);
}

static bool rand_1(void *dest, size_t size) {
  // Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of
  // random noise). This can take a long time to generate random data if the result of analogRead(0)
  // doesn't change very frequently.
  unsigned char *p = (unsigned char*) dest;
  while (size) {
    uint8_t val = 0;
    for (unsigned i = 0; i < 8; ++i) {
      int init = analogRead(36);
      int count = 0;
      while (analogRead(36) == init) {
        ++count;
      }

      if (count == 0) {
         val = (val << 1) | (init & 0x01);
      } else {
         val = (val << 1) | (count & 0x01);
      }
    }
    *p = val;
    ++p;
    --size;
  }
  // NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
  return true;
}

static int RNG(unsigned char *dest, unsigned int size) {
  return (int) rand_1((void*)dest, size);
}

void runLMS() {
  param_set_t lm_array[1];
  param_set_t ots_array[1];
  lm_array[0] = LMS_SHA256_N32_H10;
  //lm_array[1] = LMS_SHA256_N32_H10;
  ots_array[0] = LMOTS_SHA256_N32_W8;
  //ots_array[1] = LMOTS_SHA256_N32_W8;

  int pubkey_size = hss_get_public_key_len( 1, lm_array, ots_array );
  int sig_size = hss_get_signature_len( 1, lm_array, ots_array );
  int privkey_size = hss_get_private_key_len( 1, lm_array, ots_array );

  unsigned char pubkey[HSS_MAX_PUBLIC_KEY_LEN];
  unsigned char *sig = (unsigned char*) malloc(sig_size);
  unsigned char privkey[HSS_MAX_PRIVATE_KEY_LEN];
  size_t aux_size = DEFAULT_AUX_DATA;
  unsigned aux_len;
    if (aux_size > 0) {
        aux_len = hss_get_aux_data_len( aux_size, 1,
                                               lm_array, ots_array);
    } else {
        aux_len = 1;
    }
  unsigned char *aux = (unsigned char*)heap_caps_malloc(aux_len,MALLOC_CAP_SPIRAM);

  Serial.println("Start key generation");
  startMillis = millis();
  hss_generate_private_key(rand_1, 1, lm_array, ots_array, NULL, privkey, pubkey, pubkey_size, aux, aux_len, 0);
  struct hss_working_key *w = hss_load_private_key(NULL, privkey, 0, aux, aux_len, 0 );
  currentMillis = millis();
  Serial.print("Key generation took (ms): ");
  Serial.println(currentMillis - startMillis);

  static const unsigned char message[65] = "This is a signature test string with a total length of 64 bytes.";
  Serial.println("Starting Signature generation...");
  struct hss_extra_info info;
  hss_init_extra_info( &info );
  digitalWrite(21,HIGH);
  startMillis = millis();
  if (hss_generate_signature( w, NULL, privkey,message, sizeof message, sig, sig_size, &info)) {
    currentMillis = millis();
    digitalWrite(21,LOW);
    Serial.print("Signature generation took (ms): ");
    Serial.println(currentMillis - startMillis);
    Serial.print("Signature size is (bytes): ");
    Serial.println(sizeof(sig));
  } else {
    Serial.println("sig gen failed");
  }

  Serial.println("Start signature verification...");
  startMillis = millis();
  if (hss_validate_signature( pubkey, message, sizeof message, sig, sig_size, 0)) {
    currentMillis = millis();
    Serial.print("Signature verification took (ms): ");
    Serial.println(currentMillis - startMillis);
  } else {
    Serial.print("sig gen failed");
  }
  Serial.print("Free stack is: ");
  Serial.println(uxTaskGetStackHighWaterMark(NULL));
  Serial.print("Free heap is: ");
  Serial.println(ESP.getFreeHeap());

  hss_free_working_key(w);
  free(aux);
  free(sig);
}

void runXMSS() {
  xmss_params params;
    uint32_t oid;
    int i;

    xmss_str_to_oid(&oid, XMSS_VARIANT);
    xmss_parse_oid(&params, oid);

    /* define key fields and msg */
    unsigned char pk[XMSS_OID_LEN + params.pk_bytes];
    unsigned char sk[XMSS_OID_LEN + params.sk_bytes];
    unsigned char *m = (unsigned char *) malloc(XMSS_MLEN);
    unsigned char *sm = (unsigned char *) malloc(params.sig_bytes + XMSS_MLEN);
    unsigned char *mout = (unsigned char *) malloc(params.sig_bytes + XMSS_MLEN);
    unsigned long long smlen;
    unsigned long long mlen;
    //randombytes(m, XMSS_MLEN);
    memcpy(m,"This is a signature test string with a total length of 64 bytes.", XMSS_MLEN);
    Serial.println("Start key generation");
    startMillis = millis();
    XMSS_KEYPAIR(pk, sk, oid);
    currentMillis = millis();
    Serial.print("Key generation took (ms): ");
    Serial.println(currentMillis - startMillis);

    Serial.println("Starting Signature generation...");
    digitalWrite(21,HIGH);
    startMillis = millis();
    XMSS_SIGN(sk, sm, &smlen, m, XMSS_MLEN);
    currentMillis = millis();
    digitalWrite(21,LOW);
    Serial.print("Signature generation took (ms): ");
    Serial.println(currentMillis - startMillis);
    Serial.print("Signature size is (bytes): ");
    Serial.println((unsigned long) smlen);

    startMillis = millis();
    i = XMSS_SIGN_OPEN(mout, &mlen, sm, smlen, pk);
    currentMillis = millis();

    if(i > 0) {
      Serial.println("Verification failed...");
    } else {
      if (mlen != XMSS_MLEN) {
        Serial.println("Message length not identical...");
      } else {
        Serial.println("Message length is identical.");
      } if (memcmp(m, mout, XMSS_MLEN)) {
        Serial.println("Message content not identical...");
      } else {
        Serial.println("Message content is identical.");
      }
      Serial.print("Signature verification took (ms): ");
      Serial.println(currentMillis - startMillis);
    }
    Serial.print("Free stack is: ");
    Serial.println(uxTaskGetStackHighWaterMark(NULL));
    Serial.print("Free heap is: ");
    Serial.println(ESP.getFreeHeap());

    free(m);
    free(sm);
    free(mout);
}

void runBlissB() {
  uint8_t seed[SHA3_512_DIGEST_LENGTH] = {0};
  entropy_t entropy;
  bliss_private_key_t private_key;
  bliss_public_key_t public_key;
  bliss_signature_t signature;
  bliss_kind_t type;
  int32_t retcode;
  char* text = "This is a signature test string with a total length of 64 bytes.";
  uint8_t* msg = (uint8_t*)text;
  size_t msg_sz = strlen(text) + 1;
  type = BLISS_B_2;
  entropy_init(&entropy, seed);

  /* key gen */
  Serial.println("Start key generation...");
  startMillis = millis();
  retcode = bliss_b_private_key_gen(&private_key, type, &entropy);
  if (retcode != BLISS_B_NO_ERROR) {
    Serial.println("bliss_b_private_key_gen failed.");
  } else {
    retcode = bliss_b_public_key_extract(&public_key, &private_key);
    currentMillis = millis();
    if (retcode != BLISS_B_NO_ERROR) {
      Serial.println("bliss_b_public_key_extract failed.");
    } else {
      Serial.print("Key generation took (ms): ");
      Serial.println(currentMillis - startMillis);
    }
  }

  /* sig gen */
  Serial.println("Start signature generation...");
  digitalWrite(21,HIGH);
  startMillis = millis();
  retcode = bliss_b_sign(&signature, &private_key, msg, msg_sz, &entropy);
  currentMillis = millis();
  digitalWrite(21,LOW);
  Serial.print("Signature generation took (ms): ");
  Serial.println(currentMillis - startMillis);
  if (retcode != BLISS_B_NO_ERROR) {
    Serial.println("bliss_b_sign failed.");
  }

  char sig_array[4191] = "a";
  bliss_signature_retrieve_params(&signature, sig_array, &public_key);
  Serial.print("Signature size is: ");
  Serial.println(sizeof(sig_array));


  /* verify */
  Serial.println("Verify Signature...");
  startMillis = millis();
  retcode = bliss_b_verify(&signature, &public_key, msg, msg_sz);
  currentMillis = millis();
  if (retcode != BLISS_B_NO_ERROR) {
    Serial.println("bliss_b_verify failed.");
  } else {
    Serial.print("Signature verification took (ms): ");
    Serial.println(currentMillis - startMillis);
  }
  Serial.print("Free stack is: ");
  Serial.println(uxTaskGetStackHighWaterMark(NULL));
  Serial.print("Free heap is: ");
  Serial.println(ESP.getFreeHeap());

  bliss_b_private_key_delete(&private_key);

  bliss_b_public_key_delete(&public_key);

  bliss_signature_delete(&signature);
}

void runEcdsa() {
  uint8_t privatek[32] = {0};
  uint8_t publick[64] = {0};
  const struct uECC_Curve_t * curve = uECC_secp256k1();
  Serial.println("Start key generation");
  uECC_set_rng(&RNG);
  startMillis = millis();
  uECC_make_key(publick, privatek, curve);
  currentMillis = millis();
  Serial.print("Key generation took (ms): ");
  Serial.println(currentMillis - startMillis);

  Serial.println("Starting Signature generation...");
  digitalWrite(21,HIGH);
  startMillis = millis();
  uint8_t hash[32];
  uint8_t sig[64] = {0};
  char const *str = "This is a signature test string with a total length of 64 bytes.";
  const size_t len = strlen(str);
  mbedtls_sha256_context sha256_ctx;
  mbedtls_sha256_init(&sha256_ctx);
  mbedtls_sha256_starts(&sha256_ctx, 0);
  mbedtls_sha256_update(&sha256_ctx, (const unsigned char *) str, len);
  mbedtls_sha256_finish(&sha256_ctx, hash);
  if (uECC_sign(privatek, hash, sizeof(hash), sig, curve)) {
    currentMillis = millis();
    digitalWrite(21,LOW);
    Serial.print("Signature generation took (ms): ");
    Serial.println(currentMillis - startMillis);
    Serial.print("Signature size is (bytes): ");
    Serial.println(sizeof(sig));
  }

  startMillis = millis();
  if (uECC_verify(publick, hash, sizeof(hash), sig, curve)) {
    currentMillis = millis();
    Serial.print("Signature verification took (ms): ");
    Serial.println(currentMillis - startMillis);
  }

  Serial.print("Free stack is: ");
  Serial.println(uxTaskGetStackHighWaterMark(NULL));
}

void runEddsa() {
  unsigned char seed[32], public_key[32], private_key[64], signature[64];
  unsigned char other_public_key[32], other_private_key[64], shared_secret[32];
  const unsigned char message[65] = "This is a signature test string with a total length of 64 bytes.";

  /* create a random seed, and a key pair out of that seed */
  Serial.println("Start key generation");
  startMillis = millis();
  if (ed25519_create_seed(seed,32)) {
    Serial.println("error while generating seed\n");
    exit(1);
  }
  ed25519_create_keypair(public_key, private_key, seed);
  currentMillis = millis();
  Serial.print("Key generation took (ms): ");
  Serial.println(currentMillis - startMillis);

  /* create signature on the message with the key pair */
  Serial.println("Starting Signature generation...");
  digitalWrite(21,HIGH);
  startMillis = millis();
  ed25519_sign(signature, message, 65, public_key, private_key);
  currentMillis = millis();
  digitalWrite(21,LOW);
  Serial.print("Signature generation took (ms): ");
  Serial.println(currentMillis - startMillis);
  Serial.print("Signature size is (bytes): ");
  Serial.println(sizeof(signature));


  /* verify the signature */
  Serial.println("Start signature verification...");
  startMillis = millis();
  if (ed25519_verify(signature, message, 65, public_key)) {
    currentMillis = millis();
    Serial.print("Signature verification took (ms): ");
    Serial.println(currentMillis - startMillis);
  } else {
    Serial.println("invalid signature");
  }
  Serial.print("Free stack is: ");
  Serial.println(uxTaskGetStackHighWaterMark(NULL));
}

void run_test(void *pvParameter) {
  Serial.println("Run Eddsa: ");
  runEddsa();
  delay(100);
  Serial.println("Run Ecdsa: ");
  runEcdsa();
  delay(100);
  Serial.println("Run BlissB2: ");
  runBlissB();
  delay(100);
  Serial.println("Run LMS: ");
  runLMS();
  delay(100);
  Serial.println("Run XMSS: ");
  runXMSS();
  Serial.println("Sleep for 1 sec...");
  digitalWrite(21,HIGH);
  delay(1000);
  digitalWrite(21,LOW);
  while(1) {

  }
}

void app_main()
{
    Serial.begin(115200);
    pinMode(21, OUTPUT);
    xTaskCreate(&run_test, "run_test", 16184, NULL, 1, NULL);
}
