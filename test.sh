# Generate 100 files with sequential content
for i in {1..1000}; do
  cat <<EOF > test_files/content/doc$i.md
---
title: "Document $i"
date: $(date -v -${i}d +%Y-%m-%d)
---

# Test Document $i

This is automatically generated content for performance testing.
- Item 1
- Item 2
- Item 3

Generated at: $(date)
EOF
done

# Add some nested directories
mkdir -p test_files/content/{blog,articles/2023}
for i in {1..20}; do
  cp test_files/content/doc$i.md test_files/content/blog/post$i.md
  cp test_files/content/doc$i.md test_files/content/articles/2023/article$i.md
done

# Total files: 100 + 20 + 20 = 140