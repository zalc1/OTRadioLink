/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

/*
 * EXTRA unit test routines for secureable-frame library code.
 *
 * Should all be runnable on any UNO-alike or emulator;
 * does not need V0p2 boards or hardware.
 *
 * Some tests here may duplicate those in the core libraries,
 * which is OK in order to double-check some core functionality.
 */

// Arduino libraries imported here (even for use in other .cpp files).

#define UNIT_TESTS

#include <Wire.h>

// Include the library under test.
#include <OTV0p2Base.h>
#include <OTRadioLink.h>
// Also testing against crypto.
#include <OTAESGCM.h>

#if F_CPU == 1000000 // 1MHz CPU indicates V0p2 board.
#define ON_V0P2_BOARD
#endif

void setup()
  {
#ifdef ON_V0P2_BOARD
  // initialize serial communications at 4800 bps for typical use with V0p2 board.
  Serial.begin(4800);
#else
  // initialize serial communications at 9600 bps for typical use with (eg) Arduino UNO.
  Serial.begin(9600);
#endif
  }



// Error exit from failed unit test, one int parameter and the failing line number to print...
// Expects to terminate like panic() with flashing light can be detected by eye or in hardware if required.
static void error(int err, int line)
  {
  for( ; ; )
    {
    Serial.print(F("***Test FAILED*** val="));
    Serial.print(err, DEC);
    Serial.print(F(" =0x"));
    Serial.print(err, HEX);
    if(0 != line)
      {
      Serial.print(F(" at line "));
      Serial.print(line);
      }
    Serial.println();
//    LED_HEATCALL_ON();
//    tinyPause();
//    LED_HEATCALL_OFF();
//    sleepLowPowerMs(1000);
    delay(1000);
    }
  }

// Deal with common equality test.
static inline void errorIfNotEqual(int expected, int actual, int line) { if(expected != actual) { error(actual, line); } }
// Allowing a delta.
static inline void errorIfNotEqual(int expected, int actual, int delta, int line) { if(abs(expected - actual) > delta) { error(actual, line); } }

// Test expression and bucket out with error if false, else continue, including line number.
// Macros allow __LINE__ to work correctly.
#define AssertIsTrueWithErr(x, err) { if(!(x)) { error((err), __LINE__); } }
#define AssertIsTrue(x) AssertIsTrueWithErr((x), 0)
#define AssertIsEqual(expected, x) { errorIfNotEqual((expected), (x), __LINE__); }
#define AssertIsEqualWithDelta(expected, x, delta) { errorIfNotEqual((expected), (x), (delta), __LINE__); }


// Check that correct version of this library is under test.
static void testLibVersion()
  {
  Serial.println("LibVersion");
#if !(0 == ARDUINO_LIB_OTRADIOLINK_VERSION_MAJOR) || !(9 == ARDUINO_LIB_OTRADIOLINK_VERSION_MINOR)
#error Wrong library version!
#endif
  }

// Check that correct versions of underlying libraries are in use.
static void testLibVersions()
  {
  Serial.println("LibVersions");
#if !(0 == ARDUINO_LIB_OTV0P2BASE_VERSION_MAJOR) || !(9 == ARDUINO_LIB_OTV0P2BASE_VERSION_MINOR)
#error Wrong library version!
#endif

#if !(0 == ARDUINO_LIB_OTAESGCM_VERSION_MAJOR) || !(2 <= ARDUINO_LIB_OTAESGCM_VERSION_MINOR)
#error Wrong library version!
#endif
  }


static const int AES_KEY_SIZE = 128; // in bits
static const int GCM_NONCE_LENGTH = 12; // in bytes
static const int GCM_TAG_LENGTH = 16; // in bytes (default 16, 12 possible)

// Test quick integrity checks, for TX and RX.
static void testFrameQIC()
  {
  Serial.println("FramQIC");
  OTRadioLink::SecurableFrameHeader sfh;
  uint8_t id[OTRadioLink::SecurableFrameHeader::maxIDLength];
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize + 1];
  // Uninitialised SecurableFrameHeader should be 'invalid'.
  AssertIsTrue(sfh.isInvalid());
  // ENCODE
  // Test various bad input combos that should be caught by QIC.
  // Can futz (some of the) inputs that should not matter...
  // Should fail with bad ID length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               OTV0P2BASE::randRNG8(),
                                               id, OTRadioLink::SecurableFrameHeader::maxIDLength + 1,
                                               2,
                                               1));
  // Should fail with bad buffer length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, 0,
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
  // Should fail with bad frame type.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_NONE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_INVALID_HIGH,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
  // Should fail with impossible body length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               252,
                                               1));
  // Should fail with impossible trailer length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               0,
                                               0));
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               0,
                                               252));
  // Should fail with impossible body + trailer length (for small frame).
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               32,
                                               32));
  // "I'm Alive!" message with 1-byte ID should succeed and be of full header length (5).
  AssertIsEqual(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1, // Minimal (non-empty) ID.
                                               0, // No payload.
                                               1));
  // Large but legal body size.
  AssertIsEqual(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1, // Minimal (non-empty) ID.
                                               32,
                                               1));
  // DECODE
  // Test various bad input combos that should be caught by QIC.
  // Can futz (some of the) inputs that should not matter...
  // Should fail with bad (too small) buffer.
  buf[0] = OTV0P2BASE::randRNG8();
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf, 0));
  // Should fail with bad (too small) frame length.
  buf[0] = 3 & OTV0P2BASE::randRNG8();
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf, sizeof(buf)));
  // Should fail with bad (too large) frame length for 'small' frame.
  buf[0] = 64;
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf, sizeof(buf)));
  // Should fail with bad (too large) frame header for the input buffer.
  const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81 }; // , 0x02, 0x00, 0x01, 0x23 };
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf1, 5));
  // Should fail with bad trailer byte (illegal 0x00 value).
  const uint8_t buf2[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0x00 };
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
  // Should fail with bad trailer byte (illegal 0xff value).
  const uint8_t buf3[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0xff };
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf3, sizeof(buf3)));
  // TODO
  // TODO
  // TODO
  }

// Test encoding of header for TX.
static void testFrameHeaderEncoding()
  {
  Serial.println("FrameHeaderEncoding");
  OTRadioLink::SecurableFrameHeader sfh;
  uint8_t id[OTRadioLink::SecurableFrameHeader::maxIDLength];
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  id[0] = 0x80;
  id[1] = 0x81;
  AssertIsEqual(6, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               0,
                                               id, 2,
                                               2,
                                               1));
  AssertIsEqual(0x08, buf[0]);
  AssertIsEqual(0x4f, buf[1]);
  AssertIsEqual(0x02, buf[2]);
  AssertIsEqual(0x80, buf[3]);
  AssertIsEqual(0x81, buf[4]);
  AssertIsEqual(0x02, buf[5]);
  // Check related parameters.
  AssertIsEqual(8, sfh.fl);
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(8, sfh.getTrailerOffset());
  //
  // Test vector 2 / example from the spec.
  //Example insecure frame, no valve, representative minimum stats {"b":1}
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //0e 4f 02 80 81 08 | 7f 11 7b 22 62 22 3a 31 | 61
  //
  //0e length of header (14) after length byte 5 + body 8 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //08 body length 8
  //7f no valve, no call for heat
  //11 stats present flag only, unreported occupancy
  //7b 22 62 22 3a 31  {"b":1  Stats: note that implicit trailing '}' is not sent.
  //61 CRC value
  id[0] = 0x80;
  id[1] = 0x81;
  AssertIsEqual(6, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               0,
                                               id, 2,
                                               8,
                                               1));
  AssertIsEqual(0x0e, buf[0]);
  AssertIsEqual(0x4f, buf[1]);
  AssertIsEqual(0x02, buf[2]);
  AssertIsEqual(0x80, buf[3]);
  AssertIsEqual(0x81, buf[4]);
  AssertIsEqual(0x08, buf[5]);
  // Check related parameters.
  AssertIsEqual(14, sfh.fl);
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(14, sfh.getTrailerOffset());
  }

// Test decoding of header for RX.
static void testFrameHeaderDecoding()
  {
  Serial.println("FrameHeaderDecoding");
  OTRadioLink::SecurableFrameHeader sfh;
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0x23 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf1, sizeof(buf1)));
  // Check decoded parameters.
  AssertIsEqual(8, sfh.fl);
  AssertIsEqual(2, sfh.getIl());
  AssertIsEqual(0x80, sfh.id[0]);
  AssertIsEqual(0x81, sfh.id[1]);
  AssertIsEqual(2, sfh.bl);
  AssertIsEqual(1, sfh.getTl());
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(8, sfh.getTrailerOffset());
  //
  // Test vector 2 / example from the spec.
  //Example insecure frame, no valve, representative minimum stats {"b":1}
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //0e 4f 02 80 81 08 | 7f 11 7b 22 62 22 3a 31 | 61
  //
  //0e length of header (14) after length byte 5 + body 8 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //08 body length 8
  //7f no valve, no call for heat
  //11 stats present flag only, unreported occupancy
  //7b 22 62 22 3a 31  {"b":1  Stats: note that implicit trailing '}' is not sent.
  //61 CRC value
  static const uint8_t buf2[] = { 0x0e, 0x4f, 0x02, 0x80, 0x81, 0x08, 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31, 0x61 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
  // Check decoded parameters.
  AssertIsEqual(14, sfh.fl);
  AssertIsEqual(2, sfh.getIl());
  AssertIsEqual(0x80, sfh.id[0]);
  AssertIsEqual(0x81, sfh.id[1]);
  AssertIsEqual(8, sfh.bl);
  AssertIsEqual(1, sfh.getTl());
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(14, sfh.getTrailerOffset());
  }

// Test CRC computation for insecure frames.
static void testNonsecureFrameCRC()
  {
  Serial.println("NonsecureFrameCRC");
  OTRadioLink::SecurableFrameHeader sfh;
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01 }; //, 0x23 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf1, sizeof(buf1)));
  AssertIsEqual(0x23, sfh.computeNonSecureFrameCRC(buf1, sizeof(buf1)));
  //
  // Test vector 2 / example from the spec.
  //Example insecure frame, no valve, representative minimum stats {"b":1}
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //0e 4f 02 80 81 08 | 7f 11 7b 22 62 22 3a 31 | 61
  //
  //0e length of header (14) after length byte 5 + body 8 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //08 body length 8
  //7f no valve, no call for heat
  //11 stats present flag only, unreported occupancy
  //7b 22 62 22 3a 31  {"b":1  Stats: note that implicit trailing '}' is not sent.
  //61 CRC value
  const uint8_t buf2[] = { 0x0e, 0x4f, 0x02, 0x80, 0x81, 0x08, 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31 }; // , 0x61 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
  AssertIsEqual(0x61, sfh.computeNonSecureFrameCRC(buf2, sizeof(buf2)));
  }

// Test encoding of entire non-secure frame for TX.
static void testNonSecureSmallFrameEncoding()
  {
  Serial.println("NonSecureSmallFrameEncoding");
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  const uint8_t id[] =  { 0x80, 0x81 };
  const uint8_t body[] = { 0x00, 0x01 };
  AssertIsEqual(9, OTRadioLink::encodeNonsecureSmallFrame(buf, sizeof(buf),
                                    OTRadioLink::FTS_BasicSensorOrValve,
                                    0,
                                    id, 2,
                                    body, 2));
  AssertIsEqual(0x08, buf[0]);
  AssertIsEqual(0x4f, buf[1]);
  AssertIsEqual(0x02, buf[2]);
  AssertIsEqual(0x80, buf[3]);
  AssertIsEqual(0x81, buf[4]);
  AssertIsEqual(0x02, buf[5]);
  AssertIsEqual(0x00, buf[6]);
  AssertIsEqual(0x01, buf[7]);
  AssertIsEqual(0x23, buf[8]);
  }

// Test simple plain-text padding for encryption.
static void testSimplePadding()
  {
  Serial.println("SimplePadding");
  uint8_t buf[OTRadioLink::ENC_BODY_SMALL_FIXED_CTEXT_SIZE];
  // Provoke failure with NULL buffer.
  AssertIsEqual(0, OTRadioLink::addPaddingTo32BTrailing0sAndPadCount(NULL, 0x1f & OTV0P2BASE::randRNG8()));
  // Provoke failure with over-long unpadded plain-text.
  AssertIsEqual(0, OTRadioLink::addPaddingTo32BTrailing0sAndPadCount(buf, 1 + OTRadioLink::ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE));  
  // Check padding in case with single random data byte (and the rest of the buffer set differently).
  // Check the entire padded result for correctness.
  const uint8_t db0 = OTV0P2BASE::randRNG8();
  buf[0] = db0;
  memset(buf+1, ~db0, sizeof(buf)-1);
  AssertIsEqual(32, OTRadioLink::addPaddingTo32BTrailing0sAndPadCount(buf, 1));
  AssertIsEqual(db0, buf[0]);
  for(int i = 30; --i > 0; ) { AssertIsEqual(0, buf[i]); }
  AssertIsEqual(30, buf[31]);
  // Ensure that unpadding works.
  AssertIsEqual(1, OTRadioLink::removePaddingTo32BTrailing0sAndPadCount(buf));
  AssertIsEqual(db0, buf[0]);
  }


// All-zeros 16-byte/128-bit key.
static const uint8_t zeroKey[16] = { };

// Test simple fixed-size NULL enc/dec behaviour.
static void testSimpleNULLEncDec()
  {
  const OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e = OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL;
  const OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d = OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleDec_NULL_IMPL;
  // Check that calling the NULL enc routine with bad args fails.
  AssertIsTrue(!e(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  static const uint8_t plaintext1[32] = { 'a', 'b', 'c', 'd', 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 };
  static const uint8_t nonce1[12] = { 'q', 'u', 'i', 'c', 'k', ' ', 6, 5, 4, 3, 2, 1 };
  static const uint8_t authtext1[2] = { 'H', 'i' };
  // Output ciphertext and tag buffers.
  uint8_t co1[32], to1[16];
  AssertIsTrue(e(NULL, zeroKey, nonce1, authtext1, sizeof(authtext1), plaintext1, co1, to1));
  AssertIsEqual(0, memcmp(plaintext1, co1, 32));
  AssertIsEqual(0, memcmp(nonce1, to1, 12));
  AssertIsEqual(0, to1[12]);
  AssertIsEqual(0, to1[15]);
  // Check that calling the NULL decc routine with bad args fails.
  AssertIsTrue(!d(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  // Decode the ciphertext and tag from above and ensure that it 'works'.
  uint8_t plaintext1Decoded[32];
  AssertIsTrue(d(NULL, zeroKey, nonce1, authtext1, sizeof(authtext1), co1, to1, plaintext1Decoded));
  AssertIsEqual(0, memcmp(plaintext1, plaintext1Decoded, 32));
  }





//static OTAESGCM::OTAES128GCMGeneric<> ed; // FIXME: ensure state is cleared afterwards.
//
//bool fixed32Be(void *,
//        const uint8_t *key, const uint8_t *iv,
//        const uint8_t *authtext, uint8_t authtextSize,
//        const uint8_t *plaintext,
//        uint8_t *ciphertextOut, uint8_t *tagOut)
//  {
//  if((NULL == key) || (NULL == iv) ||
//     (NULL == plaintext) || (NULL == ciphertextOut) || (NULL == tagOut)) { return(false); } // ERROR
//  return(ed.gcmEncrypt(key, iv, plaintext, 32, authtext, authtextSize, ciphertextOut, tagOut));
//  }
//
//bool fixed32Bd(void *state,
//        const uint8_t *key, const uint8_t *iv,
//        const uint8_t *authtext, uint8_t authtextSize,
//        const uint8_t *ciphertext, const uint8_t *tag,
//        uint8_t *plaintextOut)
//  {
//  if((NULL == key) || (NULL == iv) ||
//     (NULL == ciphertext) || (NULL == tag) || (NULL == tag)) { return(false); } // ERROR
//  return(ed.gcmDecrypt(key, iv, ciphertext, 32, authtext, authtextSize, tag, plaintextOut));
//  }

// Test a simple fixed-size enc/dec function pair.
// Aborts with Assert...() in case of failure.
static void runSimpleEncDec(const OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e,
                            const OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d)
  {
  // Check that calling the NULL enc routine with bad args fails.
  AssertIsTrue(!e(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  static const uint8_t plaintext1[32] = { 'a', 'b', 'c', 'd', 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 };
  static const uint8_t nonce1[12] = { 'q', 'u', 'i', 'c', 'k', ' ', 6, 5, 4, 3, 2, 1 };
  static const uint8_t authtext1[2] = { 'H', 'i' };
  // Output ciphertext and tag buffers.
  uint8_t co1[32], to1[16];
  AssertIsTrue(e(NULL, zeroKey, nonce1, authtext1, sizeof(authtext1), plaintext1, co1, to1));
  // Check that calling the NULL decc routine with bad args fails.
  AssertIsTrue(!d(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  // Decode the ciphertext and tag from above and ensure that it 'works'.
  uint8_t plaintext1Decoded[32];
  AssertIsTrue(d(NULL, zeroKey, nonce1, authtext1, sizeof(authtext1), co1, to1, plaintext1Decoded));
  AssertIsEqual(0, memcmp(plaintext1, plaintext1Decoded, 32));
  }

// Test basic access to crypto features.
// Check basic operation of the simple fixed-sized encode/decode routines.
static void testCryptoAccess()
  {
  Serial.println("CryptoAccess");
  // NULL enc/dec.
  runSimpleEncDec(OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL,
                  OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleDec_NULL_IMPL);
//  // AES-GCM 128-bit key enc/dec with static state.
//  runSimpleEncDec(fixed32Be,
//                  fixed32Bd);
  // AES-GCM 128-bit key enc/dec.
  runSimpleEncDec(OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS,
                  OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS);
  }

// Check using NIST GCMVS test vector.
// Test via fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS interface.
// See http://csrc.nist.gov/groups/STM/cavp/documents/mac/gcmvs.pdf
// See http://csrc.nist.gov/groups/STM/cavp/documents/mac/gcmtestvectors.zip
//
//[Keylen = 128]
//[IVlen = 96]
//[PTlen = 256]
//[AADlen = 128]
//[Taglen = 128]
//
//Count = 0
//Key = 298efa1ccf29cf62ae6824bfc19557fc
//IV = 6f58a93fe1d207fae4ed2f6d
//PT = cc38bccd6bc536ad919b1395f5d63801f99f8068d65ca5ac63872daf16b93901
//AAD = 021fafd238463973ffe80256e5b1c6b1
//CT = dfce4e9cd291103d7fe4e63351d9e79d3dfd391e3267104658212da96521b7db
//Tag = 542465ef599316f73a7a560509a2d9f2
//
// keylen = 128, ivlen = 96, ptlen = 256, aadlen = 128, taglen = 128, count = 0
static void testGCMVS1ViaFixed32BTextSize()
  {
  Serial.println("GCMVS1ViaFixed32BTextSize");
  // Inputs to encryption.
  static const uint8_t input[32] = { 0xcc, 0x38, 0xbc, 0xcd, 0x6b, 0xc5, 0x36, 0xad, 0x91, 0x9b, 0x13, 0x95, 0xf5, 0xd6, 0x38, 0x01, 0xf9, 0x9f, 0x80, 0x68, 0xd6, 0x5c, 0xa5, 0xac, 0x63, 0x87, 0x2d, 0xaf, 0x16, 0xb9, 0x39, 0x01 };
  static const uint8_t key[AES_KEY_SIZE/8] = { 0x29, 0x8e, 0xfa, 0x1c, 0xcf, 0x29, 0xcf, 0x62, 0xae, 0x68, 0x24, 0xbf, 0xc1, 0x95, 0x57, 0xfc };
  static const uint8_t nonce[GCM_NONCE_LENGTH] = { 0x6f, 0x58, 0xa9, 0x3f, 0xe1, 0xd2, 0x07, 0xfa, 0xe4, 0xed, 0x2f, 0x6d };
  static const uint8_t aad[16] = { 0x02, 0x1f, 0xaf, 0xd2, 0x38, 0x46, 0x39, 0x73, 0xff, 0xe8, 0x02, 0x56, 0xe5, 0xb1, 0xc6, 0xb1 };
  // Space for outputs from encryption.
  uint8_t tag[GCM_TAG_LENGTH]; // Space for tag.
  uint8_t cipherText[max(32, sizeof(input))]; // Space for encrypted text.
  // Instance to perform enc/dec.
  // Do encryption via simplified interface.
  AssertIsTrue(OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS(NULL,
            key, nonce,
            aad, sizeof(aad),
            input,
            cipherText, tag));
  // Check some of the cipher text and tag.
//            "0388DACE60B6A392F328C2B971B2FE78F795AAAB494B5923F7FD89FF948B  61 47 72 C7 92 9C D0 DD 68 1B D8 A3 7A 65 6F 33" :
  AssertIsEqual(0xdf, cipherText[0]);
  AssertIsEqual(0x91, cipherText[5]);
  AssertIsEqual(0xdb, cipherText[sizeof(cipherText)-1]);
  AssertIsEqual(0x24, tag[1]);
  AssertIsEqual(0xd9, tag[14]);
  // Decrypt via simplified interface...
  uint8_t inputDecoded[32];
  AssertIsTrue(OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS(NULL,
            key, nonce,
            aad, sizeof(aad),
            cipherText, tag,
            inputDecoded));
  AssertIsEqual(0, memcmp(input, inputDecoded, 32));
  }

// Test encoding/encryption then decoding/decryption of entire secure frame.
static void testSecureSmallFrameEncoding()
  {
  Serial.println("SecureSmallFrameEncoding");
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
  //Example 3: secure, no valve, representative minimum stats {"b":1}).
  //In this case the frame sequence number is zero,
  //and the ID is 0xaa 0xaa 0xaa 0xaa (transmitted) with the next ID bytes 0x55 0x55.
  //ResetCounter = 42
  //TxMsgCounter = 793
  //(Thus nonce/IV: aa aa aa aa 55 55 00 00 2a 00 03 19)
  //
  //3e cf 04 aa aa aa aa 20 | b3 45 f9 29 69 57 0c b8 28 66 14 b4 f0 69 b0 08 71 da d8 fe 47 c1 c3 53 83 48 88 03 7d 58 75 75 | 00 00 2a 00 03 19 97 5b da df 92 08 42 b8 c1 3b dc 02 76 54 cb 8d 80 
  //
  //
  //3e  length of header (62) after length byte 5 + (encrypted) body 32 + trailer 32
  //cf  'O' secure OpenTRV basic frame
  //04  0 sequence number, ID length 4
  //aa  ID byte 1
  //aa  ID byte 2
  //aa  ID byte 3
  //aa  ID byte 4
  //20  body length 32 (after padding and encryption)
  //    Plaintext body (length 8): 0x7f 0x11 { " b " : 1 
  //    Padded: 7f 11 7b 22 62 22 3a 31 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 17
  //b3 45 f9 ... 58 75 75  32 bytes of encrypted body
  //00 00 2a  reset counter
  //00 03 19  message counter
  //97 5b da ... 54 cb 8d  16 bytes of authentication tag
  //80  enc/auth type/format indicator.
  // Preshared ID prefix; only an initial part/prefix of this goes on the wire in the header.
  const uint8_t id[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55 };
  // IV/nonce starting with first 6 bytes of preshared ID, then 6 bytes of counter.
  const uint8_t iv[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x00, 0x00, 0x2a, 0x00, 0x03, 0x19 };
  // 'O' frame body with some JSON stats.
  const uint8_t body[] = { 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31 };
  const uint8_t seqNum = 0;
  const uint8_t encodedLength = OTRadioLink::encodeSecureSmallFrameRaw(buf, sizeof(buf),
                                    OTRadioLink::FTS_BasicSensorOrValve,
                                    seqNum,
                                    id, 4,
                                    body, sizeof(body),
                                    iv,
                                    OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS,
                                    NULL, zeroKey);
  AssertIsEqual(63, encodedLength);
  AssertIsTrue(encodedLength <= sizeof(buf));
  //3e cf 04 aa aa aa aa 20 | ...
  AssertIsEqual(0x3e, buf[0]);
  AssertIsEqual(0xcf, buf[1]);
  AssertIsEqual(0x04, buf[2]);
  AssertIsEqual(0xaa, buf[3]);
  AssertIsEqual(0xaa, buf[4]);
  AssertIsEqual(0xaa, buf[5]);
  AssertIsEqual(0xaa, buf[6]);
  AssertIsEqual(0x20, buf[7]);
  //... b3 45 f9 29 69 57 0c b8 28 66 14 b4 f0 69 b0 08 71 da d8 fe 47 c1 c3 53 83 48 88 03 7d 58 75 75 | ...
  AssertIsEqual(0xb3, buf[8]); // 1st byte of encrypted body.
  AssertIsEqual(0x75, buf[39]); // 32nd/last byte of encrypted body.
  //... 00 00 2a 00 03 19 97 5b da df 92 08 42 b8 c1 3b dc 02 76 54 cb 8d 80
  AssertIsEqual(0x00, buf[40]); // 1st byte of counters.
  AssertIsEqual(0x00, buf[41]);
  AssertIsEqual(0x2a, buf[42]);
  AssertIsEqual(0x00, buf[43]); 
  AssertIsEqual(0x03, buf[44]);
  AssertIsEqual(0x19, buf[45]); // Last byte of counters.
  AssertIsEqual(0x97, buf[46]); // 1st byte of tag.
  AssertIsEqual(0x8d, buf[61]); // 16th/last byte of tag.
  AssertIsEqual(0x80, buf[62]); // enc format.
  // To decode, emulating RX, structurally validate unpack the header and extract the ID.
  OTRadioLink::SecurableFrameHeader sfhRX;
  AssertIsTrue(0 != sfhRX.checkAndDecodeSmallFrameHeader(buf, encodedLength));
  // (Nominally a longer ID and key is looked up with the ID in the header, and an iv built.)
  uint8_t decodedBodyOutSize;
  uint8_t decryptedBodyOut[OTRadioLink::ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE];
  // Should decode and authenticate correctly.
  AssertIsTrue(0 != OTRadioLink::decodeSecureSmallFrameRaw(&sfhRX,
                                        buf, encodedLength,
                                        OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                        NULL, zeroKey, iv,
                                        decryptedBodyOut, sizeof(decryptedBodyOut), decodedBodyOutSize));
  // Body content should be correctly decrypted and extracted.
  AssertIsEqual(sizeof(body), decodedBodyOutSize);
  AssertIsEqual(0, memcmp(body, decryptedBodyOut, sizeof(body)));
  // Check that flipping any single bit should make the decode fail
  // unless it leaves all info (seqNum, id, body) untouched.
  const uint8_t loc = OTV0P2BASE::randRNG8() % encodedLength;
  const uint8_t mask = (uint8_t) (0x100U >> (1+(OTV0P2BASE::randRNG8()&7)));
  buf[loc] ^= mask;
//  Serial.println(loc);
//  Serial.println(mask);
  AssertIsTrue((0 == sfhRX.checkAndDecodeSmallFrameHeader(buf, encodedLength)) ||
               (0 == OTRadioLink::decodeSecureSmallFrameRaw(&sfhRX,
                                        buf, encodedLength,
                                        OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                        NULL, zeroKey, iv,
                                        decryptedBodyOut, sizeof(decryptedBodyOut), decodedBodyOutSize)) ||
               ((seqNum == sfhRX.getSeq()) && (sizeof(body) == decodedBodyOutSize) && (0 == memcmp(body, decryptedBodyOut, sizeof(body))) && (0 == memcmp(id, sfhRX.id, 4))));
  }

// TODO: test with EEPROM ID source (id_ == NULL) ...
// TODO: add EEPROM prefill static routine and pad 1st trailing byte with 0xff.


// To be called from loop() instead of main code when running unit tests.
// Tests generally flag an error and stop the test cycle with a call to panic() or error().
void loop()
  {
  static int loopCount = 0;

  // Allow the terminal console to be brought up.
  for(int i = 3; i > 0; --i)
    {
    Serial.print(F("Tests starting... "));
    Serial.print(i);
    Serial.println();
    delay(1000);
    }
  Serial.println();


  // Run the tests, fastest / newest / most-fragile / most-interesting first...
  testLibVersion();
  testLibVersions();

  testFrameQIC();
  testFrameHeaderEncoding();
  testFrameHeaderDecoding();
  testNonsecureFrameCRC();
  testNonSecureSmallFrameEncoding();
  testSimplePadding();
  testSimpleNULLEncDec();
  testCryptoAccess();
  testGCMVS1ViaFixed32BTextSize();
  testSecureSmallFrameEncoding();



  // Announce successful loop completion and count.
  ++loopCount;
  Serial.println();
  Serial.print(F("%%% All tests completed OK, round "));
  Serial.print(loopCount);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.flush();
  delay(2000);
  }
