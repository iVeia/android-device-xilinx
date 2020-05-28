all: packed

tarball:
	cp -r cups_update_base cups_update
	tar cvf cups_update.tar cups_update/ ; gzip -9 cups_update.tar
	rm -r cups_update

packed: tarball
	cp packed_extractor.sh cups.sh
	cat cups_update.tar.gz >> cups.sh
	./make_manifest.sh

