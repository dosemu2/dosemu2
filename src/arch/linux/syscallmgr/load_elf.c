/*
 * Copyright (C) 1995, Bjorn Ekwall <bj0rn@blox.se>
 *
 * See the file "COPYING" for your rights.
 *
 * This file is based on info on elf support by Eric Youngdale <eric@aib.com>
 * That info was abused into this source by Bjorn Ekwall <bj0rn@blox.se>
 *
 * Fixes for 1.2.8 from H.J.Lu <hjl@nynexst.com>
 * Many ELF and other fixes from:
 * 	James Bottomley <J.E.J.Bottomley@damtp.cambridge.ac.uk>
 * mc68000 additions:
 *	Andreas Schwab <schwab@issan.informatik.uni-dortmund.de>
 *
 * More fixes (ELF SHN_UNDEF et al) by  Andreas Schwab in February 1996
 * sparc additions:
 *	Eddie C. Dost <ecd@skynet.be>
 */
#include "insmod.h"

static Elf32_Shdr *sections;
static char **secref;
static int *got;
static int n_got;
static int bss_seg;
static int bss_size;
static char *loaded;

#define GOTTEN 0x80

static void
build_got(void)
{
#ifdef __i386__
	Elf32_Ehdr *epnt = &header;
	Elf32_Shdr *spnt;
	Elf32_Rel *rpnt;
	struct symbol *sp;
	int *next_got;
	int symtab_index;
	int i;
	int n_rel;

	next_got = got = (int *)loaded;
	n_got = 0;

#ifdef DEBUG
	insmod_debug ("got = 0x%08x", got);
#endif
	for (spnt = sections, i = 0; i < epnt->e_shnum; ++i, ++spnt) {
		if (spnt->sh_type == SHT_REL) {
			rpnt = (Elf32_Rel *)secref[i];
			n_rel= sections[i].sh_size / sections[i].sh_entsize;

			while (n_rel-- > 0) {
				if (ELF32_R_TYPE(rpnt->r_info) == R_386_GOT32) {
					symtab_index = ELF32_R_SYM(rpnt->r_info);
					sp = &symtab[symtab_index];
					if (!(sp->u.e.st_other & GOTTEN)) {
						sp->u.e.st_other |= GOTTEN;
						*next_got++ = (int)sp;
						++n_got;
						codesize += sizeof(int);
#ifdef DEBUG
	insmod_debug ("Created got-entry for '%s'",
		(char *) sp->u.e.st_name);
#endif
					}
				}
				++rpnt;
			}
		}
	}

	loaded = (char *)next_got;

	/* clean up */
	for (i = 0, next_got = got; i < n_got; ++i, ++next_got) {
		sp = (struct symbol *)(*next_got);
		sp->u.e.st_other &= ~GOTTEN;
	}
#endif
}

static void
elf_relocate(unsigned int loadaddr, void *secpnt, int n_rel)
{
	struct symbol *sp = NULL;
	unsigned int *reloc_addr;
	unsigned int symbol_addr;
	unsigned int real_reloc_addr;
	int i;
	int reloc_type;
	int symtab_index;
#ifdef __i386__
	int *gp;
	int g;
	int got_addr = (int)got - (int)textseg + addr;
	Elf32_Rel *rpnt = (Elf32_Rel *) secpnt;
#endif
#if defined(__mc68000__) || defined(__sparc__)
	Elf32_Rela *rpnt = (Elf32_Rela *) secpnt;
#endif

#ifdef DEBUG
	insmod_debug ("loadaddr = 0x%08x", loadaddr);
#endif
	for (i = 0; i < n_rel; ++i, ++rpnt) {
		symbol_addr = 0;
		reloc_addr = (int *)(loadaddr + (int)rpnt->r_offset);
		real_reloc_addr = loadaddr - (int)textseg + (int)addr +
				(int)rpnt->r_offset;
		reloc_type = ELF32_R_TYPE(rpnt->r_info);
		symtab_index = ELF32_R_SYM(rpnt->r_info);

#ifdef DEBUG
		insmod_debug ("symtab_index = 0x%02x ", symtab_index);
#endif
		if(symtab_index) {
			sp = &symtab[symtab_index];
#ifdef DEBUG
			insmod_debug ("%s %x %x %d %d %x (%x) ", 
				sp->u.e.st_name,
				(int)sp->u.e.st_value,
				(int)sp->u.e.st_size,
				(int)sp->u.e.st_info,
				(int)sp->u.e.st_other,
				(int)sp->u.e.st_shndx,
				(int)reloc_addr );    
#endif
			/* kludge done at loading:
			 * st_value = offset from image start!
			 */
			if (sp->u.e.st_shndx &&
			    sp->u.e.st_shndx < SHN_LORESERVE &&
			    secref[sp->u.e.st_shndx]) {
				symbol_addr = (unsigned int)sp->u.e.st_value +
						addr;
			}
			else {
				symbol_addr = (unsigned int)sp->u.e.st_value;
				/* Hack for mod_use_count! */
				if (ELF32_ST_BIND(sp->u.e.st_info) == STB_LOCAL)
					symbol_addr += addr;
			}

			if(!symbol_addr) {
				insmod_error ("Unable to resolve symbol %s",
					(char *)sp->u.e.st_name);
				exit(1);
			}
		}
#ifdef DEBUG
		insmod_debug ("symbol value = 0x%08x (%d)",
			symbol_addr, reloc_type);
#endif
		switch(reloc_type) {
#ifdef __i386__
		case R_386_32:
			*reloc_addr += symbol_addr;
			break;

		case R_386_PLT32:
		case R_386_PC32:
			*reloc_addr += symbol_addr - real_reloc_addr;
			break;

		case R_386_GLOB_DAT:
		case R_386_JMP_SLOT:
			*reloc_addr = symbol_addr;
			break;

		case R_386_RELATIVE:
			*reloc_addr += (int)loadaddr - (int)textseg + addr;
			break;

		case R_386_GOTPC:
			*reloc_addr += got_addr - real_reloc_addr;
			break;

		case R_386_GOT32:
			for (g = 0, gp = got; g < n_got; ++g, ++gp) {
				if (*gp == (int)sp)
					break;
			}
			if (g == n_got) {
				insmod_error ("'%s' not in got",
					(char *)sp->u.e.st_name);
				exit(1);
			}
			*reloc_addr += g * sizeof(int) - real_reloc_addr;
			break;


		case R_386_GOTOFF:
			*reloc_addr += symbol_addr - got_addr;
			break;
#endif
#ifdef __mc68000__
		case R_68K_NONE:
			break;

		case R_68K_8:
			*(char *) reloc_addr = symbol_addr + rpnt->r_addend;
			break;
		case R_68K_16:
			*(short *) reloc_addr = symbol_addr + rpnt->r_addend;
			break;
		case R_68K_32:
			*reloc_addr = symbol_addr + rpnt->r_addend;
			break;

		case R_68K_PC8:
			*(char *) reloc_addr = (symbol_addr + rpnt->r_addend
						- real_reloc_addr);
			break;
		case R_68K_PC16:
			*(short *) reloc_addr = (symbol_addr + rpnt->r_addend
						 - real_reloc_addr);
			break;
		case R_68K_PC32:
			*reloc_addr = (symbol_addr + rpnt->r_addend
				       - real_reloc_addr);
			break;

		case R_68K_RELATIVE:
			*reloc_addr += ((int) loadaddr - (int) textseg + addr
					+ rpnt->r_addend);
			break;
#endif
#ifdef __sparc__
		case R_SPARC_NONE:
			break;

		case R_SPARC_8:
			*reloc_addr &= ~(0xff);
			*reloc_addr |= (symbol_addr + rpnt->r_addend) & 0xff;
			break;

		case R_SPARC_16:
			*reloc_addr &= ~(0xffff);
			*reloc_addr |= (symbol_addr + rpnt->r_addend) & 0xffff;
			break;

		case R_SPARC_32:
			*reloc_addr = symbol_addr + rpnt->r_addend;
			break;

		case R_SPARC_DISP8:
			insmod_error("unhandled reloc SPARC_DISP8, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_DISP16:
			insmod_error("unhandled reloc SPARC_DISP16, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_DISP32:
			insmod_error("unhandled reloc SPARC_DISP32, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);

		case R_SPARC_WDISP30:
			*reloc_addr &= ~(0x3fffffff);
			*reloc_addr |= (symbol_addr - real_reloc_addr) >> 2;
			break;

		case R_SPARC_WDISP22:
			insmod_error("unhandled reloc SPARC_WDISP22, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);

		case R_SPARC_HI22:
			*reloc_addr &= ~(0x3fffff);
			*reloc_addr |= (symbol_addr + rpnt->r_addend) >> 10;
			break;

		case R_SPARC_22:
			insmod_error("unhandled reloc SPARC_22, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_13:
			insmod_error("unhandled reloc SPARC_13, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);

		case R_SPARC_LO10:
			*reloc_addr &= ~(0x3ff);
			*reloc_addr |= (symbol_addr + rpnt->r_addend) & 0x3ff;
			break;

		case R_SPARC_GOT10:
			insmod_error("unhandled reloc SPARC_GOT10, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_GOT13:
			insmod_error("unhandled reloc SPARC_GOT13, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_GOT22:
			insmod_error("unhandled reloc SPARC_GOT22, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_PC10:
			insmod_error("unhandled reloc SPARC_PC10, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_PC22:
			insmod_error("unhandled reloc SPARC_PC22, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_WPLT30:
			insmod_error("unhandled reloc SPARC_WPLT30, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_COPY:
			insmod_error("unhandled reloc SPARC_COPY, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_GLOB_DAT:
			insmod_error("unhandled reloc SPARC_GLOB_DAT, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_JMP_SLOT:
			insmod_error("unhandled reloc SPARC_JMP_SLOT, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_RELATIVE:
			insmod_error("unhandled reloc SPARC_RELATIVE, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
		case R_SPARC_UA32:
			insmod_error("unhandled reloc SPARC_UA32, ");
			insmod_error("mail to ecd@skynet.be\n");
			exit (1);
#endif

		default:
			insmod_error ("Unable to handle reloc type %d",
				reloc_type);
			exit(1);
		}
	}
}

void
relocate_elf(FILE *fp, long offset)
{
	Elf32_Ehdr *epnt = &header;
	Elf32_Shdr *spnt;
	int *next_got;
	unsigned int loadaddr;
	int i;
	int n_rel;

#ifdef DEBUG
	insmod_debug ("textseg = 0x%08x", textseg);
#endif

	/* JEJB: add relocation to all the relevant pointers */
	got += offset;
	secref[bss_seg] += offset;
	for(i=0; i < epnt->e_shnum; ++i)
	        if (sections[i].sh_type == SHT_PROGBITS)
		        secref[i] += offset;

	/* JEJB: zero the bss (now it's actually allocated) */
	memset(secref[bss_seg], bss_size, 0);

	for (spnt = sections, i = 0; i < epnt->e_shnum; ++i, ++spnt) {
#ifdef __i386__
		if (spnt->sh_type == SHT_REL) {
			loadaddr = (unsigned int)secref[spnt->sh_info];
			n_rel= sections[i].sh_size / sections[i].sh_entsize;
			elf_relocate(loadaddr, (void *) secref[i], n_rel);
		}
#endif
#if defined(__mc68000__) || defined(__sparc__)
		if (spnt->sh_type == SHT_RELA) {
			loadaddr = (unsigned int)secref[spnt->sh_info];
			n_rel= sections[i].sh_size / sections[i].sh_entsize;
			elf_relocate(loadaddr, (void *) secref[i], n_rel);
		}
#endif
	}

	/* fix up got */
	for (i = 0, next_got = got; i < n_got; ++i, ++next_got) {
		struct symbol *sp;
		int symbol_addr;

		sp = (struct symbol *)(*next_got);
		if (sp->u.e.st_shndx &&
		    sp->u.e.st_shndx < SHN_LORESERVE &&
		    secref[sp->u.e.st_shndx]) {
			symbol_addr = (unsigned int)sp->u.e.st_value + addr;
			}
		else {
			symbol_addr = (unsigned int)sp->u.e.st_value;
			/* Hack for mod_use_count! */
			if (ELF32_ST_BIND(sp->u.e.st_info) == STB_LOCAL)
				symbol_addr += addr;
		}
		*next_got = symbol_addr;
	}
}

char *
load_elf(FILE *fp)
{
	Elf32_Ehdr *epnt = &header;
	Elf32_Shdr *spnt;
	long filesize;
	int i;
	int n;
	struct symbol *sp;

	got = 0;
	n_got = 0;

	if (epnt->e_phnum != 0)
		return "can't handle program headers...";

	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);

	/* large enough, even when considering alignments */
	textseg = (char *)ckalloc(filesize);
	memset(textseg, 0, filesize);
	bss_size = 0;
	bss_seg = -1;
	loaded = textseg;

	/* load the section headers */
	sections = (Elf32_Shdr *)ckalloc(epnt->e_shentsize * epnt->e_shnum);
	secref = (char **)ckalloc(epnt->e_shnum * sizeof(char *));

	fseek(fp, epnt->e_shoff, SEEK_SET);
	fread((char *)sections, epnt->e_shentsize * epnt->e_shnum, 1, fp);
	if (feof(fp) || ferror(fp))
		return "Error reading ELF section headers";

	for (spnt = sections, i = 0; i < epnt->e_shnum; ++i, ++spnt) {
		int align_me = spnt->sh_addralign?
				(spnt->sh_addralign - 1): 0;

		switch (spnt->sh_type) {
		case SHT_NULL:
		case SHT_NOTE:
			secref[i] = NULL;
			break; /* IGNORE */

		case SHT_PROGBITS:
			/* Build the (aligned) image right now */
			loaded = (char *)(((int)loaded + align_me) & ~align_me);
			secref[i] = loaded;

			fseek(fp, spnt->sh_offset, SEEK_SET);
			fread(loaded, spnt->sh_size, 1, fp);
			if (feof(fp) || ferror(fp))
				return "Error reading ELF PROGBITS section";

			loaded += spnt->sh_size;
			break;

		case SHT_NOBITS:
			bss_size += spnt->sh_size;
			/* We pick the first SHT_NOBITS section.
			 * for .bss segment. */
			if (bss_seg >= 0)
				return "Error - multiple bss sections";
			bss_seg = i;
			/* If it has something in it. */
			if (bss_size > 0) {
				/* If it is the first time. */
				if (bss_seg == i)
					secref[i] = (char *)align_me; /* kludge */
				else
					/* Set up an error condition. */
					secref[i] = (char *) -1;
			}
			else
				secref[i] = NULL;
			break;

		case SHT_SYMTAB:
			if (nsymbols)
			        return "Can't handle >1 symbol section";

			nsymbols = spnt->sh_size / spnt->sh_entsize;
			secref[i] = ckalloc(nsymbols * sizeof (*symtab));
			symtab = (struct symbol*)secref[i];

			fseek(fp, spnt->sh_offset, SEEK_SET);
			for (n = nsymbols, sp = symtab ; n > 0 ; --n) {
				fread(&sp->u.e, sizeof sp->u.e, 1, fp);
				sp->child[0] = sp->child[1] = NULL;
				++sp;
			}

			/* mark the section index of the symbol table's string
			 * table (might not be loaded yet, so convert to an
			 * address after all sections are loaded) */
			stringtab = (char *)spnt->sh_link;
			if (feof(fp) || ferror(fp))
				return "Error reading ELF SYMTAB section";
			break;

		case SHT_STRTAB:
			secref[i] = (char *)ckalloc(spnt->sh_size);

			fseek(fp, spnt->sh_offset, SEEK_SET);
			fread(secref[i], spnt->sh_size, 1, fp);
			if (feof(fp) || ferror(fp))
				return "Error reading ELF STRTAB section";
			break;

		case SHT_REL:
#ifdef __i386__
			secref[i] = (char *)ckalloc(spnt->sh_size);

			fseek(fp, spnt->sh_offset, SEEK_SET);
			fread(secref[i], spnt->sh_size, 1, fp);
			if (feof(fp) || ferror(fp))
				return "Error reading ELF REL section";
			break;
#else
			return "can't handle section REL";
#endif

		case SHT_RELA:
#if defined(__mc68000__) || defined(__sparc__)
			secref[i] = (char *)ckalloc(spnt->sh_size);

			fseek(fp, spnt->sh_offset, SEEK_SET);
			fread(secref[i], spnt->sh_size, 1, fp);
			if (feof(fp) || ferror(fp))
				return "Error reading ELF RELA section";
			break;
#else
			return "can't handle section RELA";
#endif

		case SHT_HASH: return "can't handle section HASH";
		case SHT_DYNAMIC: return "can't handle section DYNAMIC";
		case SHT_SHLIB: return "can't handle section SHLIB";
		case SHT_DYNSYM: return "can't handle section DYNSYM";
		case SHT_NUM: return "can't handle section NUM";
		default: return "can't handle section > 12";
		}
	}

	/* convert stringtab to address */
	stringtab = secref[(long)stringtab];

	for (spnt = sections, i = 0; i < epnt->e_shnum; ++i, ++spnt) {
		(char *)(spnt->sh_name) = secref[epnt->e_shstrndx] 
		                          + spnt->sh_name;
#ifdef DEBUG
		insmod_debug("Section %d: %s", i, (char *)spnt->sh_name);
#endif /* DEBUG */
	}

	if (bss_size) {
		int align_me = (int)secref[bss_seg];

		loaded = (char *)(((int)loaded + align_me) & ~align_me);
		secref[bss_seg] = loaded;
		loaded += bss_size;
	}

	for (n = nsymbols, sp = symtab ; --n >= 0 ; sp++) {
		/* Hmmm... */
		if ((sp->u.e.st_info == 0) && (sp->u.e.st_shndx != SHN_UNDEF))
			sp->u.e.st_info = STT_OBJECT; /* as good as any... */
		/* look up name and add sp to splay tree */
		findsym(stringtab + sp->u.e.st_name, sp, strncmp);
		if ((ELF32_ST_BIND(sp->u.e.st_info) != STB_LOCAL) &&
		    (sp->u.e.st_shndx != SHN_UNDEF))
			symother(sp) = DEF_BY_MODULE; /* abuse: mark extdef */
		else {
			symother(sp) = 0; /* abuse: mark extref */
		}

		/* Kludge: I want the value (= address) to be relative
		 * the start of the newly build image.
		 * I rely on the secref mapping to be correct...
		 */
		if ((sp->u.e.st_shndx != SHN_UNDEF) &&
		    (sp->u.e.st_shndx < header.e_shnum) && /* always >= 0 ! */
		    secref[sp->u.e.st_shndx]) {

			(unsigned int)sp->u.e.st_value +=
				(int)(secref[sp->u.e.st_shndx]) -
				(int)textseg;
		}
		else if (sp->u.e.st_shndx == SHN_COMMON) {
			/* Wow! I'm adding this _after_ the BSS segment... */
			sp->u.e.st_shndx = bss_seg; /* sorry... */
			if (bss_seg == -1)
				return "Ouch! Mail <bj0rn@blox.se> and say that bss_seg was -1";
			/* was the bss segment empty? In that case, fake one */
			if (secref[bss_seg] == NULL)
				secref[bss_seg] = loaded;

			/*
			 * The st_value field represents alignment constraints,
			 * not an actual location.  The size represents the
			 * correct size of the variable.
			 */
			if( ((unsigned int) loaded & (sp->u.e.st_value - 1)) != 0 ) {
				loaded += sp->u.e.st_value;
				loaded = (char *) ((unsigned int) loaded &
					~(sp->u.e.st_value - 1));
			}
			(unsigned int)sp->u.e.st_value =
				(int)loaded - (int)textseg;
			loaded = textseg +
				sp->u.e.st_value + sp->u.e.st_size;
		}
	}

	loaded = (char *)(((int)loaded + 3) & ~3);
	bss_size = loaded - secref[bss_seg];
	progsize = codesize = loaded - textseg; 
	aout_flag = 0; /* i.e.: if it's not a.out, it _has_ to be ELF... */
	if (defsym(strncmp, "_GLOBAL_OFFSET_TABLE_", loaded - textseg, N_BSS | N_EXT, TRANSIENT))
		build_got();

	if (verbose) {
		for (spnt = sections, i = 0; i < epnt->e_shnum; ++i, ++spnt) {
			if (secref[i])
				insmod_debug ("Section %d: (%s) at 0x%lx",
					i, (char *)spnt->sh_name, (long)secref[i]);
		}
		insmod_debug ("textseg = 0x%lx", (long)textseg);
		insmod_debug ("bss_size = %d", bss_size);
		insmod_debug ("last byte = 0x%lx", (long)loaded);
		insmod_debug ("module size = %d", progsize);
	}

	return (char *)0;
}
