
#define kTRJIS				-1
#define kTREUC_JP			-2
#define kTRShiftJIS			-3
#define kTRJISX0208_1978	-4
#define kTRJISX0208_1983	-5
#define kTRJISX0212_1990	-6
#define kTRJISX0201_76kana	-7

/* character sets for ISO-2022-JP (kTRJIS) */
enum {
    kASCII, 
    kJISX0201_1976,
    kJISX0208_1978,
    kJISX0208_1983,
    kJISX0201_1976Kana,                         // [Not Std ISO-2022-JP]
    kJISX0212_1990                              // [Not Std ISO-2022-JP]
};
