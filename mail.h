#pragma once

#include "types.h"
#include "fs.h"

bool load_mail();

bool   mail_getstat(const char*, fs_stat*);
size_t mail_sread(const char*, void*, size_t, file_offset*);
size_t mail_swrite(const char*, const void*, size_t);