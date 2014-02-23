diff --git a/src/dosext/mfs/mfs.c b/src/dosext/mfs/mfs.c
index 63c973a..23eea9e 100644
--- a/src/dosext/mfs/mfs.c
+++ b/src/dosext/mfs/mfs.c
@@ -3285,7 +3285,7 @@ dos_fs_redirect(state_t *state)
   char *filename1;
   char *filename2;
   unsigned dta;
-  long s_pos=0;
+  off_t s_pos=0;
   unsigned int devptr;
   u_char attr;
   u_char subfunc;
@@ -3442,7 +3442,7 @@ dos_fs_redirect(state_t *state)
   case READ_FILE:
     {				/* 0x08 */
       int return_val;
-      int itisnow;
+      off_t itisnow;
 
       cnt = WORD(state->ecx);
       if (open_files[sft_fd(sft)].name == NULL) {
@@ -3461,8 +3461,8 @@ dos_fs_redirect(state_t *state)
 	SETWORD(&(state->ecx), 0);
 	return (TRUE);
       }
-      Debug0((dbg_fd, "Actual pos %d\n",
-	      itisnow));
+      Debug0((dbg_fd, "Actual pos %u\n",
+	      (unsigned int)itisnow));
 
       ret = dos_read(fd, dta, cnt);
 
