t_alsa ()
{
	dep_paths alsa
	if test_header 'alsa/asoundlib'; then
		ldflags="-lasound -lm -ldl -lpthread"
		echo t
	fi
}

t_ao ()
{
	dep_paths ao
	if test_header 'ao/ao'; then
		ldflags="-ld -lao"
		echo t
	fi
}

t_fifo ()
{
	echo t
}

t_mvp ()
{
	echo t
}

t_oss ()
{
	dep_paths oss
	test_header 'sys/soundcard' && echo t
}

t_pulse ()
{
	dep_paths pulse
	test_header 'pulse/simple' && echo 't'
}

t_shout ()
{
	dep_paths shout
	ok=
	if test "$PKGCONFIG" != "no" && `$PKGCONFIG --exists shout`; then
		cflags="`$PKGCONFIG --variable=cflags_only shout`"
		cflags="$cflags `$PKGCONFIG --variable=cppflags shout`"
		ldflags="`$PKGCONFIG --libs shout`"
		ok=t
	else
		test -z "$sc" && sc="`which shoutconfig`"
		if test `$sc --package` = "libshout"; then
			cflags="`$sc --cflags-only`"
			cflags="$cflags `$sc --cppflags shout`"
			ldflags="$ldflags `$sc --libs`"
			ok=t
		fi
	fi
	# freebsd 6.1 + shout 2.2 port seems to leave pthread out
	case "$uname_s" in
	freebsd*)
		case "$cflags" in
		*-D_THREAD_SAFE*)
			ldflags="$ldflags -lpthread"
			;;
		esac
	;;
	esac

	echo $ok
}

t_sun ()
{
	dep_paths sun
	test_header 'sys/audioio' && echo t
}

