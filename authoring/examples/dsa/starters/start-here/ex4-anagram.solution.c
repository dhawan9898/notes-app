int is_anagram(const char *a, const char *b) {
    int count[26] = {0};
    for (; *a; a++) count[tolower((unsigned char)*a) - 'a']++;
    for (; *b; b++) count[tolower((unsigned char)*b) - 'a']--;
    for (int i = 0; i < 26; i++)
        if (count[i]) return 0;
    return 1;
}
