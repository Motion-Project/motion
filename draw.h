int initialize_chars(void);
void get_text_dimensions(const char *const str, const int big_chars, int *const width, int *const height);
int draw_text(unsigned char *image, unsigned int startx, unsigned int starty, unsigned int width, const char *text, unsigned int factor);
