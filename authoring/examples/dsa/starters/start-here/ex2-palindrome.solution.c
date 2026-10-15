int is_palindrome(const char *s) {
    int i = 0, j = (int)strlen(s) - 1;
    while (i < j) {
        while (i < j && !isalpha((unsigned char)s[i])) i++;
        while (i < j && !isalpha((unsigned char)s[j])) j--;
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)s[j])) return 0;
        i++;
        j--;
    }
    return 1;
}
