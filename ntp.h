/******************************************************************************************
 * See:
 *    Network Time Protocol Version 4: Protocol and Algorithms Specification
 *    https://datatracker.ietf.org/doc/html/rfc5905
 ******************************************************************************************/

/*
LI Leap Indicator (leap): 2-bit integer warning of an impending leap
   second to be inserted or deleted in the last minute of the current
   month with values defined in Figure 9.

           +-------+----------------------------------------+
           | Value | Meaning                                |
           +-------+----------------------------------------+
           | 0     | no warning                             |
           | 1     | last minute of the day has 61 seconds  |
           | 2     | last minute of the day has 59 seconds  |
           | 3     | unknown (clock unsynchronized)         |
           +-------+----------------------------------------+
*/
const uint8_t LEAP_INDICATOR_NO_WARN = 0;
/*
  VN Version Number (version): 3-bit integer representing the NTP version number, currently 4.
*/
const uint8_t NTP_VERSION_NUMBER = 4;

/*
Mode (mode): 3-bit integer representing the mode, with values defined in Figure 10.
                      +-------+--------------------------+
                      | Value | Meaning                  |
                      +-------+--------------------------+
                      | 0     | reserved                 |
                      | 1     | symmetric active         |
                      | 2     | symmetric passive        |
                      | 3     | client                   |
                      | 4     | server                   |
                      | 5     | broadcast                |
                      | 6     | NTP control message      |
                      | 7     | reserved for private use |
                      +-------+--------------------------+

                       Figure 10: Association Modes
*/
const uint8_t NTP_MODE_CLIENT = 3;

// See: https://datatracker.ietf.org/doc/html/rfc5905#section-7.3
/*
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9  0  1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|LI | VN  |Mode |    Stratum    |     Poll      |   Precision    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Root  Delay                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Root  Dispersion                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Reference Identifier                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                                |
|                    Reference Timestamp (64)                    |
|                                                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                                |
|                    Originate Timestamp (64)                    |
|                                                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                                |
|                     Receive Timestamp (64)                     |
|                                                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                                |
|                     Transmit Timestamp (64)                    |
|                                                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Key Identifier (optional) (32)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                                |
|                                                                |
|                 Message Digest (optional) (128)                |
|                                                                |
|                                                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 Figure 1.  NTP Packet Header
*/
typedef struct
{
  uint8_t li_vn_mode;
  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;

  uint32_t root_delay;
  uint32_t root_dispersion;
  uint32_t ref_id;

  uint32_t reference_timestamp_sec;
  uint32_t reference_timestamp_frac;

  uint32_t origin_timestamp_sec;
  uint32_t origin_timestamp_frac;

  uint32_t receive_timestamp_sec;
  uint32_t receive_timestamp_frac;

  uint32_t transmit_timestamp_sec;
  uint32_t transmit_timestamp_frac;

  // Don't care about extension fields, key identifier, or digest (we just won't read them)
} ntp_packet_header;

// Delta number of seconds between Unix UTC epoch (1970-01-01 00:00:00) and NTP epoch (1900-01-01 00:00:00)
// ie 70 years
const unsigned long long SEVENTY_YEARS_IN_SECONDS = 2208988800ull;