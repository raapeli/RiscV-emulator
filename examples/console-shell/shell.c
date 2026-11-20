#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONSOLE_DATA (*(volatile uint8_t *)0x10000000)
#define CONSOLE_STATUS (*(volatile uint8_t *)0x10000001)

static void print(const char *text) {
  while (*text)
    CONSOLE_DATA = *text++;
}

static bool starts_with(const char *line, const char *prefix) {
  while (*prefix) {
    if (*line++ != *prefix++)
      return false;
  }
  return true;
}

static void run_command(const char *line) {
  if (line[0] == '\0')
    return;

  if (starts_with(line, "help") && line[4] == '\0') {
    print("help\necho TEXT\n");
  } else if (starts_with(line, "echo") && (line[4] == '\0' || line[4] == ' ')) {
    if (line[4] == ' ')
      print(line + 5);
    print("\n");
  } else {
    print("Unknown command\n");
  }
}

void shell(void) {
  char line[128];

  for (;;) {
    size_t length = 0;
    bool invalid = false;
    print("> ");

    for (;;) {
      while (!CONSOLE_STATUS) {
      }
      uint8_t byte = CONSOLE_DATA;
      if (byte == '\n')
        break;
      if (byte == '\r')
        continue;

      if (length == sizeof(line) - 1 || byte == 0)
        invalid = true;
      if (!invalid)
        line[length++] = byte;
    }

    line[length] = '\0';
    if (invalid)
      print("Invalid input\n");
    else
      run_command(line);
  }
}
