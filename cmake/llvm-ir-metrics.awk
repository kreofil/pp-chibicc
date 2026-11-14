# Descriptive counts for the IR produced by the optimization demo. Count
# instructions at LLVM's two-space indentation; switch operand continuations
# and labels are not instructions. These are static counts, not execution cost.
function function_name(line) {
  sub(/^[^@]*@/, "", line)
  sub(/\(.*/, "", line)
  gsub(/"/, "", line)
  return line
}

function finish_function(  i, row) {
  row = pipeline "\t" name
  for (i = 1; i <= 8; i++) {
    row = row "\t" (counts[i] + 0)
    totals[i] += counts[i]
    counts[i] = 0
  }
  rows[++functions] = row
}

/^define / {
  name = function_name($0)
  inside = 1
  next
}

inside && /^}/ {
  finish_function()
  inside = 0
  next
}

inside && /^  [^ \t;\]}]/ {
  line = $0
  sub(/^  /, "", line)
  sub(/^%[^=]*=[ \t]*/, "", line)
  split(line, words, /[ \t]+/)
  op = words[1]
  if (op == "tail" || op == "musttail" || op == "notail")
    op = words[2]
  counts[1]++
  if (op == "alloca") counts[2]++
  if (op == "load") counts[3]++
  if (op == "store") counts[4]++
  if (op == "phi") counts[5]++
  if (op == "call" || op == "invoke" || op == "callbr") {
    # C/PPC symbols in this demo contain no whitespace or parentheses.
    if (match(line, /@[^ ()]+[ \t]*\(/)) {
      callee = substr(line, RSTART, RLENGTH)
      if (callee !~ /^@"?llvm\./)
        counts[6]++
      if (callee ~ /^@"?llvm\.mem(set|cpy|move)\./)
        counts[8]++
    } else {
      counts[7]++
    }
  }
}

END {
  printf "%s\tMODULE", pipeline
  for (i = 1; i <= 8; i++)
    printf "\t%d", totals[i]
  printf "\n"
  for (i = 1; i <= functions; i++)
    print rows[i]
}
