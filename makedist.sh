make distclean
./autogen.sh
./configure --enable-mpd-mad --enable-mpd-id3tag
make 
make dist
