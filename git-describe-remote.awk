#!/bin/awk -f
BEGIN {
  FS = "[ \t/^]+"
  while ("git ls-remote " ARGV[1] "| sort -Vk2" | getline) {
    nosha = 0
    if ($2 == "HEAD")
      sha = substr($1, 1, 7)
    if (!tag && $3 == "tags") {
      tag = $4
      tsha = substr($1, 1, 7)
      if (tsha == sha)
        nosha = 1
    }
  }
  printf(nosha == 0 ? "%s-g%s\n" : "%s\n", tag, sha)
}
