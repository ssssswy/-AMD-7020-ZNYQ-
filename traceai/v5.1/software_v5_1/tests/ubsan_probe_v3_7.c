/* Runtime availability probe only; not a decoder test or firmware member. */
int main(int argc, char **argv) {
    volatile int value = argc;
    (void)argv;
    return value * 2 == 2 ? 0 : 1;
}
