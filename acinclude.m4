AC_DEFUN([MISDN_CHECK_AF_ISDN], [

	ac_af_isdn=-1

	AC_ARG_WITH(AF_ISDN,
	    AC_HELP_STRING([--with-AF_ISDN=PNR], [alternative AF_ISDN protocol number, needed if AF_ISDN is not defined]),
	    [
	       ac_af_isdn="$withval"
	    ]
	)

	AC_COMPILE_IFELSE(
		AC_LANG_PROGRAM([[#include <sys/socket.h>]],
			[[int xdummy = AF_ISDN;]]
		),[
			AC_COMPUTE_INT(AF_ISDN_VAL, AF_ISDN, [
				AC_INCLUDES_DEFAULT()
				#include <sys/socket.h>
			],[
				AC_MSG_ERROR([cannot evaluate value of AF_ISDN])
			])
			if test $ac_af_isdn -gt -1 -a $ac_af_isdn != $AF_ISDN_VAL
			then
				AC_MSG_WARN([Overwriting default AF_ISDN value $AF_ISDN_VAL with $ac_af_isdn])
			fi 
		],[
			AF_ISDN_VAL=-1
			if test $ac_af_isdn -lt 0
			then
				AC_MSG_ERROR([AF_ISDN undefined and need to be set with --with-AF_ISDN=PROTOCOLNUMBER])
			fi
		]
	)
	MISDN_AF_ISDN_VAL=$ac_af_isdn
	AC_SUBST(MISDN_AF_ISDN_VAL)
	AC_SUBST(AF_ISDN_VAL)
])
