# BUG FIXES

For out-of-the box python version in Ubuntu 18.04 in Docker the encoding of the file needs to be changed to utf-8.
e.g. line 56:
    ```with open(args.output, 'w', encoding='utf-8') as f: <- with open(args.output, 'w')```
