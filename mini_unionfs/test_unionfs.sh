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

P1_PASS=0; P1_FAIL=0
P2_PASS=0; P2_FAIL=0
P3_PASS=0; P3_FAIL=0
P4_PASS=0; P4_FAIL=0

pass() { echo -e "${GREEN}PASSED${NC}"; }
fail() { echo -e "${RED}FAILED${NC}"; }

p1_pass() { pass; ((P1_PASS++)); }
p1_fail() { fail; ((P1_FAIL++)); }
p2_pass() { pass; ((P2_PASS++)); }
p2_fail() { fail; ((P2_FAIL++)); }
p3_pass() { pass; ((P3_PASS++)); }
p3_fail() { fail; ((P3_FAIL++)); }
p4_pass() { pass; ((P4_PASS++)); }
p4_fail() { fail; ((P4_FAIL++)); }

echo "========================================"
echo "   Mini-UnionFS Test Suite"
echo "========================================"

# --- Pre-flight checks ---
if [ ! -f "$FUSE_BINARY" ]; then
    echo -e "${RED}ERROR: binary '$FUSE_BINARY' not found. Run 'make' first.${NC}"
    exit 1
fi

ABS_PATH="$(cd "$(dirname "$0")" && pwd)"
if echo "$ABS_PATH" | grep -qE '^/mnt/[a-zA-Z](/|$)'; then
    echo -e "${RED}ERROR: Running from a Windows drive path: $ABS_PATH${NC}"
    echo -e "${YELLOW}FUSE cannot mount over NTFS/DrvFS filesystems in WSL.${NC}"
    echo ""
    echo "  Copy the project into your Linux home directory:"
    echo "    cp -r \"\$(pwd)\" ~/mini_unionfs"
    echo "    cd ~/mini_unionfs"
    echo "    make && ./test_unionfs.sh"
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

# ============================================================
# PERSON 1 — Core Resolution & Layer Logic
# Responsibilities: Layer stacking, path resolution,
#                  whiteout interpretation, upper dir prep
# ============================================================
echo "----------------------------------------"
echo "  Person 1: Core Resolution & Layer Logic"
echo "----------------------------------------"

# P1-T1: Layer Visibility (resolve_path finds lower-layer file)
# [Also covers College Test 1]
echo -n "P1-T1: Layer visibility (lower file readable via resolve_path)... "
if grep -q "base_only_content" "$MOUNT_DIR/base.txt" 2>/dev/null; then p1_pass; else p1_fail; fi

# P1-T2: Upper layer takes precedence over lower (layer stacking logic)
echo -n "P1-T2: Upper layer precedence over lower (layer stacking)... "
echo "upper_version" > "$UPPER_DIR/base.txt"
sleep 0.2
if grep -q "upper_version" "$MOUNT_DIR/base.txt" 2>/dev/null; then p1_pass; else p1_fail; fi

# P1-T3: Whiteout interpretation — deleted file is hidden from mount
# [Also covers College Test 3 — part 1: file hidden]
echo -n "P1-T3: Whiteout interpretation (deleted file hidden in mount)... "
rm "$MOUNT_DIR/delete_me.txt" 2>/dev/null
sleep 0.2
if [ ! -f "$MOUNT_DIR/delete_me.txt" ]; then p1_pass; else p1_fail; fi

# P1-T4: Whiteout marker file created in upper (ensure_upper_dir + .wh.*)
# [Also covers College Test 3 — part 2: marker + lower preserved]
echo -n "P1-T4: Whiteout marker .wh.delete_me.txt created in upper... "
if [ -f "$UPPER_DIR/.wh.delete_me.txt" ] && [ -f "$LOWER_DIR/delete_me.txt" ]; then p1_pass; else p1_fail; fi

echo ""

# ============================================================
# PERSON 2 — Metadata & Directory Management
# Responsibilities: getattr, readdir, mkdir, rmdir
# ============================================================
echo "----------------------------------------"
echo "  Person 2: Metadata & Directory Management"
echo "----------------------------------------"

# P2-T1: getattr returns ENOENT for missing file
echo -n "P2-T1: getattr on missing file returns ENOENT... "
if ! stat "$MOUNT_DIR/does_not_exist.txt" 2>/dev/null; then p2_pass; else p2_fail; fi

# P2-T2: readdir shows merged listing from both layers
echo -n "P2-T2: readdir shows merged listing (upper + lower files)... "
echo "upper_only" > "$UPPER_DIR/upper_file.txt"
sleep 0.2
HAS_LOWER=$(ls "$MOUNT_DIR" 2>/dev/null | grep -c "cow_file.txt")
HAS_UPPER=$(ls "$MOUNT_DIR" 2>/dev/null | grep -c "upper_file.txt")
if [ "$HAS_LOWER" -eq 1 ] && [ "$HAS_UPPER" -eq 1 ]; then p2_pass; else p2_fail; fi

# P2-T3: readdir produces no duplicate entries
echo -n "P2-T3: readdir no duplicate entries in listing... "
COUNT=$(ls "$MOUNT_DIR" 2>/dev/null | sort | uniq -d | wc -l)
if [ "$COUNT" -eq 0 ]; then p2_pass; else p2_fail; fi

# P2-T4: mkdir creates directory in upper layer
echo -n "P2-T4: mkdir creates directory in upper layer... "
mkdir "$MOUNT_DIR/newdir" 2>/dev/null
sleep 0.2
if [ -d "$UPPER_DIR/newdir" ] && [ -d "$MOUNT_DIR/newdir" ]; then p2_pass; else p2_fail; fi

echo ""

# ============================================================
# PERSON 3 — Read Operations + Copy-on-Write
# Responsibilities: read, open (CoW trigger), copy_to_upper, release
# ============================================================
echo "----------------------------------------"
echo "  Person 3: Read Operations + Copy-on-Write"
echo "----------------------------------------"

# P3-T1: Copy-on-Write — modified content appears in upper, not lower
# [Also covers College Test 2]
echo -n "P3-T1: CoW — modified content written to upper layer... "
echo "modified_content" >> "$MOUNT_DIR/cow_file.txt" 2>/dev/null
sleep 0.2
IN_MOUNT=$(grep -c "modified_content" "$MOUNT_DIR/cow_file.txt"  2>/dev/null)
IN_UPPER=$(grep -c "modified_content" "$UPPER_DIR/cow_file.txt"  2>/dev/null)
IN_LOWER=$(grep -c "modified_content" "$LOWER_DIR/cow_file.txt"  2>/dev/null)
if [ "$IN_MOUNT" -eq 1 ] && [ "$IN_UPPER" -eq 1 ] && [ "$IN_LOWER" -eq 0 ]; then p3_pass; else p3_fail; fi

# P3-T2: Lower layer file untouched after CoW (copy_to_upper preserves original)
echo -n "P3-T2: Lower file original content preserved after CoW... "
if grep -q "original_content" "$LOWER_DIR/cow_file.txt" 2>/dev/null; then p3_pass; else p3_fail; fi

# P3-T3: Read from lower subdirectory — open + read path through lower layer
echo -n "P3-T3: Read lower subdirectory file (open resolves lower path)... "
if [ -d "$MOUNT_DIR/subdir" ] && grep -q "lower_subdir_file" "$MOUNT_DIR/subdir/nested.txt" 2>/dev/null; then p3_pass; else p3_fail; fi

# P3-T4: CoW into subdirectory — copy_to_upper auto-creates parent dirs
echo -n "P3-T4: CoW into subdir (copy_to_upper creates parent in upper)... "
echo "modified_nested" >> "$MOUNT_DIR/subdir/nested.txt" 2>/dev/null
sleep 0.2
IN_UPPER_SUB=$(grep -c "modified_nested" "$UPPER_DIR/subdir/nested.txt" 2>/dev/null)
IN_LOWER_SUB=$(grep -c "modified_nested" "$LOWER_DIR/subdir/nested.txt" 2>/dev/null)
if [ "$IN_UPPER_SUB" -eq 1 ] && [ "$IN_LOWER_SUB" -eq 0 ]; then p3_pass; else p3_fail; fi

echo ""

# ============================================================
# PERSON 4 — Write + Delete + Whiteout
# Responsibilities: write, create, unlink, whiteout creation
# ============================================================
echo "----------------------------------------"
echo "  Person 4: Write + Delete + Whiteout"
echo "----------------------------------------"

# P4-T1: create — new file goes to upper layer and is readable via mount
echo -n "P4-T1: create new file lands in upper layer and is readable... "
echo "brand_new" > "$MOUNT_DIR/newfile.txt" 2>/dev/null
sleep 0.2
if [ -f "$UPPER_DIR/newfile.txt" ] && grep -q "brand_new" "$MOUNT_DIR/newfile.txt" 2>/dev/null; then p4_pass; else p4_fail; fi

# P4-T2: write inside newly created directory (create + write + ensure_upper_dir)
echo -n "P4-T2: write file inside newly mkdir'd directory... "
echo "inside_newdir" > "$MOUNT_DIR/newdir/file.txt" 2>/dev/null
sleep 0.2
if [ -f "$UPPER_DIR/newdir/file.txt" ] && grep -q "inside_newdir" "$MOUNT_DIR/newdir/file.txt" 2>/dev/null; then p4_pass; else p4_fail; fi

# P4-T3: unlink upper-only file — deleted directly, no whiteout needed
echo -n "P4-T3: unlink upper-only file (direct delete, no whiteout)... "
echo "temp" > "$UPPER_DIR/upper_only_temp.txt"
rm "$MOUNT_DIR/upper_only_temp.txt" 2>/dev/null
sleep 0.2
if [ ! -f "$MOUNT_DIR/upper_only_temp.txt" ] && \
   [ ! -f "$UPPER_DIR/upper_only_temp.txt" ]; then p4_pass; else p4_fail; fi

# P4-T4: unlink lower-layer file inside subdir — whiteout created, lower preserved
echo -n "P4-T4: unlink lower subdir file (whiteout created, lower intact)... "
rm "$MOUNT_DIR/subdir/lower_only.txt" 2>/dev/null
sleep 0.2
if [ ! -f "$MOUNT_DIR/subdir/lower_only.txt" ] && \
   [ -f "$LOWER_DIR/subdir/lower_only.txt" ]; then p4_pass; else p4_fail; fi

echo ""

# --- Teardown ---
fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null
wait $FUSE_PID 2>/dev/null
rm -rf "$TEST_DIR"

# --- Per-person summary ---
echo "========================================"
echo "   Results Per Person"
echo "========================================"
echo -e "Person 1 (Core Resolution):      ${GREEN}$P1_PASS passed${NC} / ${RED}$P1_FAIL failed${NC}"
echo -e "Person 2 (Metadata & Dirs):       ${GREEN}$P2_PASS passed${NC} / ${RED}$P2_FAIL failed${NC}"
echo -e "Person 3 (Read + CoW):            ${GREEN}$P3_PASS passed${NC} / ${RED}$P3_FAIL failed${NC}"
echo -e "Person 4 (Write + Delete):        ${GREEN}$P4_PASS passed${NC} / ${RED}$P4_FAIL failed${NC}"
echo "----------------------------------------"
TOTAL_PASS=$((P1_PASS + P2_PASS + P3_PASS + P4_PASS))
TOTAL_FAIL=$((P1_FAIL + P2_FAIL + P3_FAIL + P4_FAIL))
TOTAL=$((TOTAL_PASS + TOTAL_FAIL))
echo -e "Overall: ${GREEN}$TOTAL_PASS passed${NC} / ${RED}$TOTAL_FAIL failed${NC} / $TOTAL total"
if [ "$TOTAL_FAIL" -eq 0 ]; then
    echo -e "${GREEN}All tests passed.${NC}"
else
    echo -e "${YELLOW}Some tests failed. Check the output above.${NC}"
fi
echo "========================================"
exit $TOTAL_FAIL