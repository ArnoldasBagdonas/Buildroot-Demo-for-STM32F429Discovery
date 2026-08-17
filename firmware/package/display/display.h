#ifndef DISPLAY_H
#define DISPLAY_H

typedef const char *(*display_source_provider_t)(void);

void display_set_source_provider(display_source_provider_t provider);
int display_applet_main(int argc, char **argv);
int display_auto_applet_main(int argc, char **argv);
int display_pattern_main(int argc, char **argv);

#endif
