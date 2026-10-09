#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A toy record format:  'R' <len:1> <payload:len>  ...  repeated; 'E' ends.
   It has a deliberate bug: the copy into the 16-byte stack buffer is bounded by len, not by the buffer. */
int parse_records(const uint8_t *data, size_t size) {
    size_t pos = 0;
    int records = 0;
    while (pos < size) {
        uint8_t tag = data[pos++];
        if (tag == 'E') break;
        if (tag != 'R') return -1;
        if (pos >= size) return -1;
        uint8_t len = data[pos++];
        if (len > size - pos) return -1;                /* length is validated against the INPUT size... */
        char buf[16];
        memcpy(buf, data + pos, len);                    /* ...but not against the 16-byte destination */
        buf[len < sizeof buf ? len : sizeof buf - 1] = 0;
        pos += len;
        records++;
    }
    return records;
}
