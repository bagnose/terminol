#include <sys/ioctl.h>
#include <stdio.h>

int main (void) {
    struct winsize winsize;
    ioctl(0, TIOCGWINSZ, &winsize);

    printf ("lines %d\n",   winsize.ws_row);
    printf ("columns %d\n", winsize.ws_col);
    printf ("xpixel %d\n",  winsize.ws_xpixel);
    printf ("ypixel %d\n",  winsize.ws_ypixel);
    return 0;
}
