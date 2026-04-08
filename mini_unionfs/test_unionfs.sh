#!/bin/bash

FUSE_BINARY="./mini_unionfs"
TEST_DIR="./unionfs_test_env"
LOWER_DIR="$TEST_DIR/lower"
UPPER_DIR="$TEST_DIR/upper"
MOUNT_DIR="$TEST_DIR/mnt"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0

pass() { echo -e "${GREEN}PASSED${NC}"; ((PASS++)); }
fail() { echo -e "${RED}FAILED${NC}"; ((FAIL++)); }

echo "========================================"
echo "   Mini-UnionFS Test Suite"
echo "========================================"

# --- Pre-flight ---
if [ ! -f "$FUSE_BINARY" ]; then
    echo -e "${RED}ERROR: binary '$FUSE_BINARY' not found. Run 'make' first.${NC}"
    exit 1
fi

# WSL guard: FUSE cannot mount over Windows (NTFS/DrvFS) filesystems.
# Detect if we are running from /mnt/[a-z] (a Windows drive in WSL).
ABS_PATH="$(cd "$(dirname "$0")" && pwd)"
if echo "$ABS_PATH" | grep -qE '^/mnt/[a-zA-Z](/|$)'; then
    echo -e "${RED}ERROR: Running from a Windows drive path: $ABS_PATH${NC}"
    echo -e "${YELLOW}FUSE cannot mount over NTFS/DrvFS filesystems in WSL.${NC}"
    echo ""
    echo "  Copy the project into your Linux home directory and run from there:"
    echo ""
    echo "    cp -r \"\$(pwd)\" ~/mini_unionfs"
    echo "    cd ~/mini_unionfs"
    echo "    make"
    echo "    ./test_unionfs.sh"
    echo ""
    exit 1
fi

# --- Setup ---
rm -rf "$TEST_DIR"
mkdir -p "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"
mkdir -p "$LOWER_DIR/subdir"

echo "base_only_content"   > "$LOWER_DIR/base.txt"
echo "to_be_deleted"       > "$LOWER_DIR/delete_me.txt"
echo "original_content"    > "$LOWER_DIR/cow_file.txt"
echo "lower_subdir_file"   > "$LOWER_DIR/subdir/nested.txt"
echo "lower_only_dir_file" > "$LOWER_DIR/subdir/lower_only.txt"

$FUSE_BINARY "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR" -f &
FUSE_PID=$!
sleep 1

if ! mountpoint -q "$MOUNT_DIR"; then
    echo -e "${RED}ERROR: mount failed. Aborting.${NC}"
    kill $FUSE_PID 2>/dev/null
    rm -rf "$TEST_DIR"
    exit 1
fi

echo ""

# --- Test 1: Layer Visibility ---
echo -n "Test 1:  Layer visibility (lower file readable)... "
if grep -q "base_only_content" "$MOUNT_DIR/base.txt" 2>/dev/null; then pass; else fail; fi

# --- Test 2: Copy-on-Write ---
echo -n "Test 2:  Copy-on-Write (modify lower file)... "
echo "modified_content" >> "$MOUNT_DIR/cow_file.txt" 2>/dev/null
sleep 0.2
IN_MOUNT=$(grep -c "modified_content" "$MOUNT_DIR/cow_file.txt"  2>/dev/null)
IN_UPPER=$(grep -c "modified_content" "$UPPER_DIR/cow_file.txt"  2>/dev/null)
IN_LOWER=$(grep -c "modified_content" "$LOWER_DIR/cow_file.txt"  2>/dev/null)
if [ "$IN_MOUNT" -eq 1 ] && [ "$IN_UPPER" -eq 1 ] && [ "$IN_LOWER" -eq 0 ]; then pass; else fail; fi

# --- Test 3: Lower file untouched after CoW ---
echo -n "Test 3:  Lower layer untouched after CoW... "
if grep -q "original_content" "$LOWER_DIR/cow_file.txt" 2>/dev/null; then pass; else fail; fi

# --- Test 4: Whiteout - file hidden from mount ---
echo -n "Test 4:  Whiteout (deleted file hidden in mount)... "
rm "$MOUNT_DIR/delete_me.txt" 2>/dev/null
sleep 0.2
if [ ! -f "$MOUNT_DIR/delete_me.txt" ]; then pass; else fail; fi

# --- Test 5: Whiteout - lower file preserved ---
echo -n "Test 5:  Whiteout (lower file still on disk)... "
if [ -f "$LOWER_DIR/delete_me.txt" ]; then pass; else fail; fi

# --- Test 6: Whiteout - marker file created ---
echo -n "Test 6:  Whiteout (marker .wh.delete_me.txt created)... "
if [ -f "$UPPER_DIR/.wh.delete_me.txt" ]; then pass; else fail; fi

# --- Test 7: Create new file ---
echo -n "Test 7:  Create new file (goes to upper layer)... "
echo "brand_new" > "$MOUNT_DIR/newfile.txt" 2>/dev/null
sleep 0.2
if [ -f "$UPPER_DIR/newfile.txt" ] && grep -q "brand_new" "$MOUNT_DIR/newfile.txt" 2>/dev/null; then pass; else fail; fi

# --- Test 8: Upper takes precedence over lower ---
echo -n "Test 8:  Upper layer precedence over lower... "
echo "upper_version" > "$UPPER_DIR/base.txt"
sleep 0.2
if grep -q "upper_version" "$MOUNT_DIR/base.txt" 2>/dev/null; then pass; else fail; fi

# --- Test 9: readdir merge ---
echo -n "Test 9:  readdir shows merged listing... "
echo "upper_only" > "$UPPER_DIR/upper_file.txt"
sleep 0.2
HAS_LOWER=$(ls "$MOUNT_DIR" 2>/dev/null | grep -c "cow_file.txt")
HAS_UPPER=$(ls "$MOUNT_DIR" 2>/dev/null | grep -c "upper_file.txt")
if [ "$HAS_LOWER" -eq 1 ] && [ "$HAS_UPPER" -eq 1 ]; then pass; else fail; fi

# --- Test 10: readdir no duplicates ---
echo -n "Test 10: readdir no duplicate entries... "
COUNT=$(ls "$MOUNT_DIR" 2>/dev/null | sort | uniq -d | wc -l)
if [ "$COUNT" -eq 0 ]; then pass; else fail; fi

# --- Test 11: mkdir ---
echo -n "Test 11: mkdir creates directory in upper layer... "
mkdir "$MOUNT_DIR/newdir" 2>/dev/null
sleep 0.2
if [ -d "$UPPER_DIR/newdir" ] && [ -d "$MOUNT_DIR/newdir" ]; then pass; else fail; fi

# --- Test 12: Create file inside new directory ---
echo -n "Test 12: Create file inside newly created directory... "
echo "inside_newdir" > "$MOUNT_DIR/newdir/file.txt" 2>/dev/null
sleep 0.2
if [ -f "$UPPER_DIR/newdir/file.txt" ] && grep -q "inside_newdir" "$MOUNT_DIR/newdir/file.txt" 2>/dev/null; then pass; else fail; fi

# --- Test 13: Subdirectory visibility ---
echo -n "Test 13: Lower subdirectory and files are visible... "
if [ -d "$MOUNT_DIR/subdir" ] && grep -q "lower_subdir_file" "$MOUNT_DIR/subdir/nested.txt" 2>/dev/null; then pass; else fail; fi

# --- Test 14: CoW into subdirectory ---
echo -n "Test 14: CoW into subdirectory (parent auto-created in upper)... "
echo "modified_nested" >> "$MOUNT_DIR/subdir/nested.txt" 2>/dev/null
sleep 0.2
IN_UPPER_SUB=$(grep -c "modified_nested" "$UPPER_DIR/subdir/nested.txt" 2>/dev/null)
IN_LOWER_SUB=$(grep -c "modified_nested" "$LOWER_DIR/subdir/nested.txt" 2>/dev/null)
if [ "$IN_UPPER_SUB" -eq 1 ] && [ "$IN_LOWER_SUB" -eq 0 ]; then pass; else fail; fi

# --- Test 15: Whiteout inside subdirectory ---
echo -n "Test 15: Whiteout inside subdirectory... "
rm "$MOUNT_DIR/subdir/lower_only.txt" 2>/dev/null
sleep 0.2
if [ ! -f "$MOUNT_DIR/subdir/lower_only.txt" ] && [ -f "$LOWER_DIR/subdir/lower_only.txt" ]; then pass; else fail; fi

# --- Test 16: getattr on non-existent file ---
echo -n "Test 16: getattr on missing file returns ENOENT... "
if ! stat "$MOUNT_DIR/does_not_exist.txt" 2>/dev/null; then pass; else fail; fi

# --- Test 17: rmdir on upper-only dir ---
echo -n "Test 17: rmdir removes upper-only directory... "
mkdir -p "$UPPER_DIR/tmpdir"
rmdir "$MOUNT_DIR/tmpdir" 2>/dev/null
sleep 0.2
if [ ! -d "$MOUNT_DIR/tmpdir" ] && [ ! -d "$UPPER_DIR/tmpdir" ]; then pass; else fail; fi

# --- Test 18: unlink upper-only file ---
echo -n "Test 18: unlink upper-only file (no whiteout needed)... "
echo "temp" > "$UPPER_DIR/upper_only_temp.txt"
rm "$MOUNT_DIR/upper_only_temp.txt" 2>/dev/null
sleep 0.2
if [ ! -f "$MOUNT_DIR/upper_only_temp.txt" ] && [ ! -f "$UPPER_DIR/upper_only_temp.txt" ]; then pass; else fail; fi

# --- Teardown ---
echo ""
fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null
wait $FUSE_PID 2>/dev/null
rm -rf "$TEST_DIR"

# --- Summary ---
echo "========================================"
TOTAL=$((PASS + FAIL))
echo -e "Results: ${GREEN}$PASS passed${NC} / ${RED}$FAIL failed${NC} / $TOTAL total"
if [ "$FAIL" -eq 0 ]; then
    echo -e "${GREEN}All tests passed.${NC}"
else
    echo -e "${YELLOW}Some tests failed. Check the output above.${NC}"
fi
echo "========================================"
exit $FAIL
