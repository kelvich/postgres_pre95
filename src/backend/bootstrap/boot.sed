#
# lex.sed - sed rules to remove conflicts between the backend interface
#		LEX scanner and the POSTQUEL LEX scanner
#
# $Header$
#
s/^yy/Int_yy/g
s/\([^a-zA-Z0-9_]\)yy/\1Int_yy/g
