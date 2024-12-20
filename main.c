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
#define ERR_MSG_BAD_RECV_TIMEOUT "Invalid receive timeout: "
#define ERR_MSG_BAD_COMMAND "Invalid command line option: "
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
char *ntp_server;
int ntp_port;
int recv_timeout;

/******************************************************************************************************
 * Print program usage information
 ******************************************************************************************************/
bool print_usage_info()
{
  printf("\r\n");
  printf("Usage:\r\n");
  printf("\t\tntp-sync -s <ntp-server-sost> -p <ntp-server-port> -t <receive timeout seconds>\r\n");
  printf("\t\tntp-sync -s oceania.pool.ntp.org\r\n");
  printf("\t\tntp-sync -s oceania.pool.ntp.org -p 123\r\n");
  printf("\t\tntp-sync -s oceania.pool.ntp.org -p 123 -t 5\r\n");
  printf("\t\tntp-sync -s 103.76.40.123\r\n");
  printf("\t\tntp-sync -s 103.76.40.123 -p 123\r\n");
  printf("\t\tntp-sync -s 103.76.40.123 -p 123 -t 10\r\n");

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
void parse_args(int argc, char *argv[], char **ntp_server, int *ntp_port, int *recv_timeout)
{
  if (argc < 2)
  {
    print_usage_info();
    exit(EXIT_FAILURE);
  }

  // Clear server name
  strncpy(*ntp_server, "\0", NTP_SERVER_BUFFER_SIZE);

  // Default to port 123
  *ntp_port = 123;

  // Default to 5 seconds
  *recv_timeout = 5;

  // Start at arg offset 1
  int i = 1;
  bool server_set = false;
  bool port_set = false;
  bool timeout_set = false;

  while (i < argc)
  {
    char *arg = argv[i++];

    if (strncmp(arg, "-h", 3) == 0)
    {
      // If help found at any position then just print usage and exit
      print_usage_info();
      exit(EXIT_SUCCESS);
    }

    if (strncmp(arg, "-s", 3) == 0)
    {
      if (server_set)
      {
        // Argument already set, so exit with error message
        exit_with_error("%s\r\n", "NTP server host (-s) specified multiple times in command line args");
      }

      // Move to arg value
      arg = argv[i++];

      // Copy server name
      strncpy(*ntp_server, arg, NTP_SERVER_BUFFER_SIZE);

      server_set = true;
    }

    else if (strncmp(arg, "-p", 3) == 0)
    {
      if (port_set)
      {
        // Argument already set, so exit with error message
        exit_with_error("%s\r\n", "NTP server port (-p) specified multiple times in command line args");
      }

      // Move to arg value
      arg = argv[i++];

      // Try and parse port to a positive integer
      if (!string_to_positive_integer(arg, ntp_port))
      {
        // Bad value, so exit with error message
        exit_with_error("%s\"%s\"\r\n", ERR_MSG_BAD_NTP_PORT, arg);
      }

      port_set = true;
    }

    else if (strncmp(arg, "-t", 3) == 0)
    {
      if (timeout_set)
      {
        // Argument already set, so exit with error message
        exit_with_error("%s\r\n", "Timeout (-t) specified multiple times in command line args");
      }

      // Move to arg value
      arg = argv[i++];

      // Try and parse timeout to a positive integer
      if (!string_to_positive_integer(arg, recv_timeout))
      {
        // Bad value, so exit with error message
        exit_with_error("%s\"%s\"\r\n", ERR_MSG_BAD_RECV_TIMEOUT, arg);
      }

      timeout_set = true;
    }

    else
    {
      // Bad command arg, so exit with error message
      exit_with_error("%s\"%s\"\r\n", ERR_MSG_BAD_COMMAND, arg);
    }
  }
}

int main(int argc, char *argv[])
{
  // First allocate server buffer, it is released hereafter in all exit paths
  ntp_server = (char *)malloc(NTP_SERVER_BUFFER_SIZE + 1);

  // parse_args will exit if there is an error setting connection settings from command line args
  parse_args(argc, argv, &ntp_server, &ntp_port, &recv_timeout);

  printf("Using server %s:%d and receive timeout of %d secs\r\n", ntp_server, ntp_port, recv_timeout);

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
  timeout.tv_sec = recv_timeout;
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
  const time_t utc_time = (time_t)(transmit_timestamp_sec - SEVENTY_YEARS_IN_SECONDS);

  const struct tm *local_time = localtime(&utc_time);

  // Print the NTP time as local time string
  // 2024-12-20 10:00:00
  char output[50];
  strftime(output, 50, "ntp_time: %Y-%m-%d %H:%M:%S\r\n", local_time);
  printf("%s", output);

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