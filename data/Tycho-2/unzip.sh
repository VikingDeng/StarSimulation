#!/bin/bash

# 解压所有 .gz 文件（包括 .tar.gz）
for gz_file in *.gz; do
  gunzip "$gz_file"
done

# 如果存在 .tar 文件，进一步解压并删除原文件
for tar_file in *.tar; do
  if [ -f "$tar_file" ]; then
    tar -xf "$tar_file" && rm "$tar_file"
  fi
done

echo "所有文件已解压并清理完成。"