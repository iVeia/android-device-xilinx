#!/system/bin/sh

DEST_TMP=/data/iVeiOTA/tmp/
# Make sure that there is no lingering stuff
rm -r -f ${DEST_TMP}/cups_update

# Create our unpack dir
mkdir -p ${DEST_TMP}

# Extract our archive to there
archive=$(grep --text --line-number 'ARCHIVE:$' $0 | cut -f1 -d:)
echo "Archive starts at $archive"
tail -n +$((archive + 1)) $0 | tar xvzf - -C ${DEST_TMP} || exit 1

# And run it, with our local file as the argument
${DEST_TMP}/cups_update/install_script.sh ${DEST_TMP}/cups_update/rand.bin
ret=$?

echo $?
echo "..."

# Then cleanup
rm -r -f ${DEST_TMP}/cups_update

echo "Exiting with $ret"
exit $ret
ARCHIVE:
