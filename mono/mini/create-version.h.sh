
	if test -d ../..//.git/svn; then svn_info="echo \"`git log --no-color --first-parent --pretty=format:%b|grep -m1 git-svn-id|sed -e 's,git-svn-id: \(.*\)@\(.*\) .*,URL: \1 Revision: \2,'`"; fi;
	if test -d /.svn; then svn_info='svn info'; fi;
	if test -n "$svn_info"; then
		(cd ../../;
			LANG=C; export LANG;
			branch=`$svn_info | grep URL | sed -e 's,.*source/\(.*\)/mono.*,/\1/mono,'`;
			version=`$svn_info | grep Revision | sed 's/.*: //'`;
			echo "#define FULL_VERSION \"$branch r$version\"";
		);
	else
		echo "#define FULL_VERSION \"tarball\"";
	fi > version.h
