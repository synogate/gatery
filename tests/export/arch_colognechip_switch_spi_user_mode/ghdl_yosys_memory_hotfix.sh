sed -rz 's/always @\*\r?\n[ ]+memory = [^\n]*\n[ ]*initial...[^\n]*\n//g' top.v.tmp > top.v
