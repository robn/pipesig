#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>

int got_chld = 0;

void chld_handler(int signo) {
  got_chld = 1;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: pipesig <command> [args]\n");
    return -1;
  }

  // create stdio pairs
  int in[2], out[2];
  if (pipe(in)) {
    perror("pipe");
    return -1;
  }
  if (pipe(out)) {
    perror("pipe");
    return -1;
  }

  // block SIGCHLD
  sigset_t mask, origmask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &mask, &origmask) < 0) {
    perror("sigprocmask");
    return -1;
  }

  // prepare SIGCHLD and SIGPIPE (non) handler
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = chld_handler;
  if (sigaction(SIGCHLD, &act, NULL) < 0) {
    perror("sigaction");
    return -1;
  }
  act.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &act, NULL) < 0) {
    perror("sigaction");
    return -1;
  }

  // start the child
  pid_t child = fork();
  if (child < 0) {
    perror("fork");
    return -1;
  }

  // child
  if (!child) {
    // close stdio
    close(0);
    close(1);

    // replace with ours
    dup2(in[1], 0);
    dup2(out[1], 1);

    return execvp(argv[1], &argv[1]);
  }

  // parent
  
  int have_parent = 1, got_pipe = 0;

  char buf[8192];
  while (1) {

    // set up to proxy for the child
    fd_set rfds;
    FD_ZERO(&rfds);
    if (have_parent)
      FD_SET(0, &rfds);
    FD_SET(out[0], &rfds);

    int r = pselect(out[0]+1, &rfds, NULL, NULL, NULL, &origmask);
    if (r < 0 && errno != EINTR) {
      perror("pselect");
      return -1;
    }

    if (got_chld)
      break;

    if (!r)
      continue;

    // parent->child
    if (FD_ISSET(0, &rfds)) {
      r = read(0, buf, 8192);
      if (r < 0) {
        perror("read");
        return -1;
      }
      if (r == 0) {
        // parent went away, don't look for stuff for them anymore
        have_parent = 0;
        continue;
      }
      if (write(in[0], buf, r) < 0) {
        perror("write");
        return -1;
      }
    }

    if (FD_ISSET(out[0], &rfds)) {
      r = read(out[0], buf, 8192);
      if (r < 0) {
        perror("read");
        return -1;
      }
      if (r == 0) {
        // child went away
        got_pipe = 1;
        break;
      }
      r = write(1, buf, r);
      if (r < 0) {
        if (errno == EPIPE) {
          got_pipe = 1;
          break;
        }
        perror("write");
        return -1;
      }
    }

  }

  // we're no longer wanted, shut the child down
  if (got_pipe) {
    // sort of a mixed bag
    close(in[0]);
    close(out[0]);
    kill(child, SIGTERM);
  }

  // if it already died, we can clean it up
  int r = -1;
  waitpid(child, &r, WNOHANG);

  // parting shot. it might already be dead, we don't care
  kill(child, SIGKILL);

  return r;
}
