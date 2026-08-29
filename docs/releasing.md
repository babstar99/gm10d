# Releasing

The version is currently defined in `src/gm10d.c` as `GM10D_VERSION`.

For a release:

1. update `GM10D_VERSION`
2. update `CHANGELOG.md`
3. run the strict build and tests
4. commit the release
5. create an annotated tag
6. generate the release archive from Git

Example:

```bash
make clean
make WERROR=1
make check WERROR=1

git tag -a v0.1.4 -m 'gm10d 0.1.4'
git archive --format=tar.gz --prefix=gm10d-0.1.4/ \
  -o gm10d-0.1.4.tar.gz v0.1.4
sha256sum gm10d-0.1.4.tar.gz > gm10d-0.1.4.tar.gz.sha256
```
