// Gallery font v2: 102 frames, lowercase Cyrillic/Latin, digits and ASCII punctuation.
// Unsupported Unicode scalar values become one question mark. Input is UTF-8.
#define GALLERY_NAME_LIMIT 15
new const GALLERY_ASCII_FRAME[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85,
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 86, 87, 88, 89, 90, 0,
    91, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 92, 93, 94, 95, 96,
    97, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 98, 99, 100, 101, 0
};
stock GalleryFont_NextFrame(const text[], &offset)
{
    new c = text[offset++] & 255, cp = c, len;
    if (c >= 0xC2 && c <= 0xDF) { cp = c & 31; len = 1; }
    else if (c >= 0xE0 && c <= 0xEF) { cp = c & 15; len = 2; }
    else if (c >= 0xF0 && c <= 0xF4) { cp = c & 7; len = 3; }
    else if (c >= 128) return 0;
    for (new i = 0; i < len; i++) {
        new next = text[offset] & 255;
        if (next < 128 || next > 191) return 0;
        cp = (cp << 6) | (next & 63); offset++;
    }
    if ((len == 1 && cp < 128) || (len == 2 && cp < 2048)
        || (len == 3 && cp < 65536) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    if (cp < 128) return GALLERY_ASCII_FRAME[cp];
    if (cp >= 1040 && cp <= 1071) cp += 32;
    if (cp == 1025 || cp == 1105) return 44;
    if (cp >= 1072 && cp <= 1077) return cp - 1072 + 38;
    if (cp >= 1078 && cp <= 1103) return cp - 1072 + 39;
    return 0;
}
stock GalleryFont_Frames(const text[], frames[], capacity = GALLERY_NAME_LIMIT)
{
    new offset, count;
    while (text[offset] && count < capacity) frames[count++] = GalleryFont_NextFrame(text, offset);
    return count;
}
