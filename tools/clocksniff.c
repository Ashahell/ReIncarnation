/* clocksniff.c - timestamp every MIDI clock (F8) that arrives at an ALSA
 * sequencer port, as the far end sees it.
 *
 * Used for the R12 N3 clock-out measurement: riapp sends F8 through
 * applemidi.out.0 -> netmidid -> LAN -> rtpmidid -> ALSA 128:2, and this
 * prints one CLOCK_MONOTONIC nanosecond stamp per F8. Statistics are
 * computed afterwards from the stamps, so nothing here smooths anything.
 *
 * usage: clocksniff <seconds> [client:port]
 *
 * build: gcc -O2 -o clocksniff tools/clocksniff.c -lasound
 *
 * CAVEAT (recorded, not hidden): the stamp is taken when the event is
 * DRAINED from the sequencer, not when the packet arrived, so two events
 * delivered in one poll wake-up read as ~0 us apart. The measured spread is
 * therefore an upper bound; rate and mean are unaffected.
 */
#include <alsa/asoundlib.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char **argv)
{
    snd_seq_t *seq;
    int port, secs = 20, src_client, src_port;
    const char *dest = "128:2";
    struct pollfd pfd;
    long long end;
    unsigned long clocks = 0, others = 0;

    if (argc > 1)
        secs = atoi(argv[1]);
    if (argc > 2)
        dest = argv[2];
    if (sscanf(dest, "%d:%d", &src_client, &src_port) != 2) {
        fprintf(stderr, "bad port '%s'\n", dest);
        return 2;
    }

    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) {
        perror("seq open");
        return 1;
    }
    snd_seq_set_client_name(seq, "clocksniff");
    port = snd_seq_create_simple_port(seq, "in",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        perror("port");
        return 1;
    }
    if (snd_seq_connect_from(seq, port, src_client, src_port) < 0) {
        fprintf(stderr, "cannot subscribe to %s\n", dest);
        return 1;
    }
    printf("# listening on %d:%d for %d s\n", src_client, src_port, secs);
    fflush(stdout);

    {
        int n = snd_seq_poll_descriptors_count(seq, POLLIN);
        struct pollfd *pfds = calloc(n, sizeof *pfds);
        snd_seq_poll_descriptors(seq, pfds, n, POLLIN);
        pfd = pfds[0];
        free(pfds);
    }

    end = now_ns() + (long long)secs * 1000000000LL;
    while (now_ns() < end) {
        int r = poll(&pfd, 1, 100);
        if (r <= 0)
            continue;
        snd_seq_event_t *ev;
        while (snd_seq_event_input(seq, &ev) > 0) {
            long long t = now_ns();
            if (ev->type == SND_SEQ_EVENT_CLOCK) {
                printf("%lld\n", t);
                clocks++;
            } else {
                others++;
            }
        }
    }
    snd_seq_close(seq);
    printf("# clocks=%lu others=%lu\n", clocks, others);
    return 0;
}
