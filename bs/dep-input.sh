t_aac ()
{
	dep_paths aac
	if test_header 'faad' && test_header 'mp4ff'; then
		ldflags="-lfaad -lmp4ff"
		echo t
	fi
}

audiofile ()
{
	dep_paths audiofile
	if test_header 'audiofile'; then
		ldflags="-lm -laudiofile"
		echo t
	fi

}

flac ()
{
}

oggvorbis ()
{
}

oggflac ()
{
}

mod ()
{
}

mpc ()
{
}

mp3 ()
{
}

tremor ()
{
}
