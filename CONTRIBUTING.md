# How to contribute

## Submitting changes

Please send a
[GitHub Pull Request to dosemu2](https://github.com/dosemu2/dosemu2/pull/new/master)
with a clear list of what you've done (read more about
[pull requests](http://help.github.com/pull-requests/)). Please follow our
coding conventions (below), make sure all of your commits are atomic (one
feature per commit) and ensure at every commit in a series the code compiles.

Always write a clear log message for your commits. One-line messages are fine
for small changes, but bigger changes should look like this:

    $ git commit -m "A brief summary of the commit
    >
    > A paragraph describing what changed and its impact."

## Coding conventions
Since dosemu2 has a long history, there are many different coding styles in
use. We'd like to standardise on a subtle variant of the Kernighan & Ritchie
style, but mass conversion is not really an option as we don't want to obscure
the commit history. New code should follow the style below, but old code
should be left in the style it is and modifications to it should be in the
same style. If an existing function is so horridly formatted as to make it
unreadable or it will be 80% changed, then it should be reformatted to the new
style and submitted as a separate commit before any changes are done. Changes
are then made in a followup commit. This will allow reviewers to view
significant changes outside of the reformatting.

Coding style is that which would be produced by
`indent -kr -nut -i2 -cli2`

```C
/*
 * large block comments
 * second line
 */

int main(int argc, char *argv[])
{
  int x, y, z = 1;

  switch (z) {
    case 1:
      printf("hello\n");
      break;

    case 0:
      printf("there\n");
      break;

    default:
      printf("unmatched switch case 0x%02x\n", z);
      return 1;
  }

  while (x == y) {
    a();
    b();

    if (x)
      single_line_1();

    if (z) {
      single_line_2();
    } else {
      multiple_line1();
      multiple_line2();
      multiple_line3();
    }
  }

  return 0;                     // no round brackets
}
```
