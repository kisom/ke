ke: kyle's text editor

This is a wordstar-inspired text editor. I wrote it mostly
in February of 2020; it's my main git editor now so it gets
used fairly often.

See the man page for more info.

It should be available via homebrew, even:

    brew tap kisom/homebrew-tap
    brew install ke

To get verbose ASAN messages:

    export LSAN_OPTIONS=verbosity=1:log_threads=1

Released under an ISC license.

Started by following along with kilo:
    https://viewsourcecode.org/snaptoken/kilo/

E.g., in the devlogs
    https://log.wntrmute.dev/2020/02/20200207
