uint32_t network_of(uint32_t ip, int prefix) {
    uint32_t mask = prefix == 0 ? 0 : 0xFFFFFFFFu << (32 - prefix);     /* a shift by 32 would be undefined */
    return ip & mask;
}
