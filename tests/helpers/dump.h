#ifndef RI_DUMP_H
#define RI_DUMP_H
#include <stdio.h>
#include <stddef.h>
static void __attribute__((unused)) dump_f32_csv(const char *path, const float *data, size_t n) {
    FILE *f = fopen(path, "w");
    size_t i;
    if (!f) return;
    for (i = 0; i < n; i++) fprintf(f, "%lu,%.9g\n", (unsigned long)i, (double)data[i]);
    fclose(f);
}
static void __attribute__((unused)) dump_events_txt(const char *path, const char *const *events, size_t n) {
    FILE *f = fopen(path, "w");
    size_t i;
    if (!f) return;
    for (i = 0; i < n; i++) fprintf(f, "%s\n", events[i] ? events[i] : "(null)");
    fclose(f);
}
#endif
