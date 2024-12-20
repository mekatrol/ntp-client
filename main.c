#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/select.h>

#include "ntp.h"

#define ERR_MSG_BAD_NTP_PORT "Invalid NTP server port: "
#define ERR_MSG_BAD_NTP_HOST "Invalid NTP server host: "
#define ERR_MSG_OPEN_SOCKET "Failed to open UDP socket\r\n"
#define ERR_MSG_CONNECT_HOST "Failed to open UDP socket\r\n"
#define ERR_MSG_CONNECT_HOST "Failed to open UDP socket\r\n"
#define ERR_SEND "Failed to send data to the host\r\n"
#define ERR_READ "Failed to read data from the host (possibly timed out)\r\n"
#define ERR_MSG_SET_RECV_TIMEOUT "Failed to set read timout\r\n"
#define ERR_SETTING_TIME_FAILED "Failed to set computer time\r\n"

#define NTP_SERVER_BUFFER_SIZE 8192

extern int errno;

// NTP server connection details
int ntp_port;
char *ntp_server;

/******************************************************************************************************
 * Print program usage information
 ******************************************************************************************************/
bool print_usage_info()
{
  printf("\r\n");
  printf("Usage (port optional, default is 123):\r\n");
  printf("\t\tntp-sync <ntp-server-host> <ntp-server-port>\r\n");
  printf("\t\tntp-sync oceania.pool.ntp.org\r\n");
  printf("\t\tntp-sync oceania.pool.ntp.org 123\r\n");
  printf("\t\tntp-sync 103.76.40.123\r\n");
  printf("\t\tntp-sync 103.76.40.123 123\r\n");

  printf("\r\n");
}

/******************************************************************************************************
 * Convert string to positive integer, return false if failed to convert (ie any non-digit in string)
 ******************************************************************************************************/
bool string_to_positive_integer(const char *s, int *value)
{
  // Initialise value to zero.
  *value = 0;

  // Enumerate string while next character is a digit.
  while (isdigit(*s))
  {
    // Accumulate positive decimal integer value
    *value = *value * 10 + (*s++ - '0');

    // If we have reached end of string then return true, value was successfully parsed.
    if (*s == '\0')
    {
      return true;
    }
  }

  // A non-digit character was found. Return false to indicate error.
  return false;
}

char *create_message(const char *format, ...)
{
  // Convert args to arg list
  va_list args;
  va_start(args, format);

  // Calculate the number of bytes needed for the message
  size_t nbytes = vsnprintf(NULL, 0, format, args);
  va_end(args);

  char *message_buffer = malloc(nbytes + 1);

  va_start(args, format);
  vsnprintf(message_buffer, nbytes, format, args);
  va_end(args);

  return message_buffer;
}

/******************************************************************************************************
 * Exits program with the specified status value
 ******************************************************************************************************/
void exit_with_status(int status)
{
  // Make sure we free NTP server memory allocation
  free(ntp_server);

  // Exit with provided status code
  exit(status);
}

/******************************************************************************************************
 * Print exit_with_error (if provided) and exit with provided status
 ******************************************************************************************************/
void exit_with_error(char *format, ...)
{
  // Convert args to arg list
  va_list args;
  va_start(args, format);

  // Calculate the number of bytes needed for the message
  size_t nbytes = vsnprintf(NULL, 0, format, args) + 1; // Number of bytes + 1 for null terminator
  va_end(args);

  char *message_buffer = malloc(nbytes);

  va_start(args, format);
  vsnprintf(message_buffer, nbytes, format, args);
  va_end(args);

  // Print error message
  printf("%s", message_buffer);

  // Free error message buffer
  free(message_buffer);

  // Exit with failure
  exit_with_status(EXIT_FAILURE);
}

/******************************************************************************************************
 * Try and process commind line args, will exit with an error result if cannot parse valid values
 ******************************************************************************************************/
void parse_args(int argc, char *argv[], int *ntp_port, char **ntp_server)
{
  // We are a simply program and expect the comman line arguments in an exact format of:
  // 1. exe name
  // 2. NTP server name
  // 3. NTP server port <optional>
  if (argc < 2 || argc > 3)
  {
    print_usage_info();
    exit(EXIT_FAILURE);
  }

  // Default to port 123
  *ntp_port = 123;

  // If there are 3 args then thirs is NTP server port so convert to integer
  if (argc >= 3)
  {
    // Reference port argument
    const char *ntp_server_port_value = argv[2];

    // Try and parse port to a positive integer
    if (!string_to_positive_integer(ntp_server_port_value, ntp_port))
    {
      // Failed to parse, so exit with error message
      exit_with_error("%s\"%s\"\r\n", ERR_MSG_BAD_NTP_PORT, ntp_server_port_value);
    }
  }

  // Copy server name
  strncpy(*ntp_server, argv[1], NTP_SERVER_BUFFER_SIZE);
}

int main(int argc, char *argv[])
{
  // First allocate server buffer, it is released hereafter in all exit paths
  ntp_server = (char *)malloc(NTP_SERVER_BUFFER_SIZE + 1);

  // parse_args will exit if there is an error setting connection settings from command line args
  parse_args(argc, argv, &ntp_port, &ntp_server);

  // Convert hostname to an IP (if not already an IP)
  struct hostent *server;
  server = gethostbyname(ntp_server);

  // Failed to get server?
  if (server == NULL)
  {
    // Failed to get host from server arg, so exit with error message
    exit_with_error("%s\"%s\"\r\n", ERR_MSG_BAD_NTP_HOST, ntp_server);
  }

  // Initialise packet header
  ntp_packet_header packet_header = {0};
  memset(&packet_header, 0, sizeof(ntp_packet_header));

  // Set version to 4 and mode to client
  packet_header.li_vn_mode = NTP_VERSION_NUMBER << 3 | NTP_MODE_CLIENT;

  // Create a UDP socket
  int udp_socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (udp_socket_fd < 0)
  {
    exit_with_error("%s\"%s\"\r\n", ERR_MSG_OPEN_SOCKET, ntp_server);
  }

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  if (setsockopt(udp_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
  {
    exit_with_error("%s\r\n", ERR_MSG_SET_RECV_TIMEOUT);
  }

  // Initialise server address
  struct sockaddr_in serv_addr;
  memset((char *)&serv_addr, 0, sizeof(serv_addr));

  // Using IP (internet protocol)
  serv_addr.sin_family = AF_INET;

  // Set server address (bind to the first IP in list)
  serv_addr.sin_addr.s_addr = *((int *)server->h_addr_list[0]);

  // Set port (after converting host byte order to TCP/IP byte order)
  serv_addr.sin_port = htons(ntp_port);

  // Send packet to NTP host
  ssize_t byte_count = sendto(udp_socket_fd, (void *)&packet_header, sizeof(ntp_packet_header), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

  if (byte_count < 0)
  {
    close(udp_socket_fd);
    exit_with_error(ERR_SEND);
  }

  // Read at most packet header number of bytes (reast of header will remain unread, but we don't care)
  socklen_t addr_len = sizeof(serv_addr);
  byte_count = recvfrom(udp_socket_fd, (char *)&packet_header, sizeof(ntp_packet_header), 0, (struct sockaddr *)&serv_addr, &addr_len);

  // Can close socket either way
  close(udp_socket_fd);

  if (byte_count < 0)
  {
    exit_with_error(ERR_READ);
  }

  // Get the transmitted timestamp seconds (ignore fraction of seconds)
  // after converting TCP/IP byte order to host byte order
  uint32_t transmit_timestamp_sec = ntohl(packet_header.transmit_timestamp_sec);

  // Get the NTP time (from NTP epoch [1990]) and convert to UTC time (from UTC epoch [1970])
  time_t utc_time = (time_t)(transmit_timestamp_sec - SEVENTY_YEARS_IN_SECONDS);

  // Print the NTP time as local time string
  printf("NTP server transmit time: %s", ctime((const time_t *)&utc_time));

  // struct timespec stime;
  // stime.tv_sec = utc_time;
  // stime.tv_nsec = 0;

  // int rc = clock_settime(CLOCK_REALTIME, &stime);
  // if (rc < 0)
  // {
  //   if (errno == EPERM)
  //   {
  //   }
  //   exit_with_error("error: %d", errno);
  // }

  // Exit with success status
  exit_with_status(EXIT_SUCCESS);
}