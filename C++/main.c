#include <stdio.h>
#include <cdio/disc.h>

int main() {
    // طباعة الأنواع المختلفة من الأقراص باستخدام discmode2str[]
    for (int i = 0; i <= CDIO_DISC_MODE_CD_I; i++) {
        printf("Disc mode %d: %s\n", i, discmode2str[i]);
    }
    
    return 0;
}