/*
 * Originally by Anonymous (as far as I know...)
 * Linux version by Bas Laarhoven <bas@vimec.nl>
 * Modified by Jon Tombs.
 *
 * Copyright (C) 1995, Bjorn Ekwall <bj0rn@blox.se>
 *
 * See the file "COPYING" for your rights.
 *
 * Fixed major bug in a.out bss variable handling: March '95, Bas.
 * sparc additions:
 *      Eddie C. Dost <ecd@skynet.be>
 */

#include "insmod.h"

static char *dataseg;
static char *bss1seg;
static char *bss2seg;

static void
aout_relocate(char *seg, int segsize, int relinfo_size, FILE *fp)
{
	struct relocation_info rel;
	unsigned long val;
	struct symbol *sp;
#ifdef __sparc__
	unsigned int symbol_addr;
	unsigned int real_reloc_addr;
#endif

	while ((relinfo_size -= sizeof rel) >= 0) {
		fread(&rel, sizeof rel, 1, fp);
#ifdef DEBUG
		printf("relocate %s:%u ", seg == textseg? "text" : "data",
			rel.r_address);
#endif
		if (rel.r_address < 0 || rel.r_address >= segsize) {
			insmod_error ("Bad relocation");
			exit(2);
		}
#ifndef __sparc__
		if (rel.r_length != 2) {
			insmod_error ("Unimplemented relocation:  r_length = %d", rel.r_length);
			exit(2);
		}
		val = * (long *) (seg + rel.r_address);
		if (rel.r_pcrel) {
			val -= addr;
#ifdef DEBUG
			printf("pc relative\n");
#endif
		}
		if (rel.r_extern) {
			if (rel.r_symbolnum >= nsymbols) {
				insmod_error ("Bad relocation");
				exit(2);
			}
			sp = symtab + rel.r_symbolnum;
			val += sp->u.n.n_value;
			if ((sp->u.n.n_type &~ N_EXT) != N_ABS)
				val += addr;
		} else if (rel.r_symbolnum != N_ABS) {
			val += addr;
		}
#else /* __sparc__ */
		val = * (long *) (seg + rel.r_address);
		symbol_addr = 0;
		real_reloc_addr = (int)seg + rel.r_address - (int)textseg + addr;
		if (rel.r_extern) {
			if (rel.r_index >= nsymbols) {
				insmod_error ("Bad relocation");
				exit(2);
			}
			sp = symtab + rel.r_index;
			symbol_addr = sp->u.n.n_value;
			if ((sp->u.n.n_type &~ N_EXT) != N_ABS)
				symbol_addr += addr;
		} else if (rel.r_index != N_ABS) {
			symbol_addr = addr;
		}

		switch (rel.r_type) {
			case RELOC_8:
				val &= ~(0xff);
				val |= (symbol_addr + rel.r_addend) & 0xff;
				break;

			case RELOC_16:
				val &= ~(0xffff);
				val |= (symbol_addr + rel.r_addend) & 0xffff;
				break;

			case RELOC_32:
				val = symbol_addr + rel.r_addend;
				break;

			case RELOC_DISP8:
				insmod_error("unhandled reloc RELOC_DISP8, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_DISP16:
				insmod_error("unhandled reloc RELOC_DISP16, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_DISP32:
				insmod_error("unhandled reloc RELOC_DISP32, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);

			case RELOC_WDISP30:
				val &= ~(0x3fffffff);
				val |= (symbol_addr - real_reloc_addr) >> 2;
				break;

			case RELOC_WDISP22:
				insmod_error("unhandled reloc RELOC_WDISP22, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);

			case RELOC_HI22:
				val &= ~(0x3fffff);
				val |= (symbol_addr + rel.r_addend) >> 10;
				break;

			case RELOC_22:
				insmod_error("unhandled reloc RELOC_22, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_13:
				insmod_error("unhandled reloc RELOC_13, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);

			case RELOC_LO10:
				val &= ~(0x3ff);
				val |= (symbol_addr + rel.r_addend) & 0x3ff;
				break;

			case RELOC_SFA_BASE:
				insmod_error("unhandled reloc RELOC_SFA_BASE, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_SFA_OFF13:
				insmod_error("unhandled reloc RELOC_SFA_OFF13, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_BASE10:
				insmod_error("unhandled reloc RELOC_BASE10, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_BASE13:
				insmod_error("unhandled reloc RELOC_BASE13, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_BASE22:
				insmod_error("unhandled reloc RELOC_BASE22, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_PC10:
				insmod_error("unhandled reloc RELOC_PC10, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_PC22:
				insmod_error("unhandled reloc RELOC_PC22, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_JMP_TBL:
				insmod_error("unhandled reloc RELOC_JMP_TBL, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_SEGOFF16:
				insmod_error("unhandled reloc RELOC_SEGOFF16, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_JMP_SLOT:
				insmod_error("unhandled reloc RELOC_JMP_SLOT, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);
			case RELOC_RELATIVE:
				insmod_error("unhandled reloc RELOC_RELATIVE, ");
				insmod_error("mail to ecd@skynet.be\n");
				exit (1);

			default:
				insmod_error ("Unable to handle reloc type %d",
					      rel.r_type);
				exit(1);
		}
#endif /* __sparc__ */
#ifdef DEBUG
		printf("val= 0x%lx\n", val);
#endif
		* (long *) (seg + rel.r_address) = val;
	}
}

void
relocate_aout(FILE *fp, long offset)
{
	struct exec *aouthdr = (struct exec *)&header;

	dataseg += offset; /* in case textseg was relocatid in insmod.c */

	fseek(fp, N_TRELOFF((*aouthdr)), SEEK_SET);
	aout_relocate(textseg, aouthdr->a_text, aouthdr->a_trsize, fp);
	fseek(fp, N_DRELOFF((*aouthdr)), SEEK_SET);
	aout_relocate(dataseg, aouthdr->a_data, aouthdr->a_drsize, fp);
}

char *
load_aout(FILE *fp)
{
	struct exec *aouthdr = (struct exec *)&header;
	struct symbol *sp;
	long filesize;
	int i;
	int len;

	aout_flag = 1;  /* needed for symvalue() macro! */
	bss2size = 0;
	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);

/* Read symbol table first because we need to know about the
 * external bss symbols before we allocate the module.
 */
	/* read in the symbol table */
	fseek(fp, N_SYMOFF((*aouthdr)), SEEK_SET);
	nsymbols = aouthdr->a_syms / sizeof (struct nlist);
	symtab = (struct symbol*) ckalloc(nsymbols * sizeof (*symtab));
        if (verbose)
		insmod_error ("reading %d symbols", nsymbols);
	for (i = nsymbols, sp = symtab ; --i >= 0 ; sp++) {
		fread(&sp->u.n, sizeof sp->u.n, 1, fp);
		sp->child[0] = sp->child[1] = NULL;
	}
	if (feof(fp) || ferror(fp))
		return "Error reading symbol table";

/* Now get the symbols strings
 */
	len = filesize - N_STROFF((*aouthdr));
	stringtab = (char*) ckalloc(len);
	fread(stringtab, len, 1, fp);
	if (feof(fp) || ferror(fp))
		return "Error reading stringtab";

	for (i = nsymbols, sp = symtab ; --i >= 0 ; sp++) {
		int pos;
		char *name = stringtab + sp->u.n.n_un.n_strx;

		pos = sp->u.n.n_un.n_strx;
		if (pos < 0 || pos >= len)
			return "Bad nlist entry";
		/* look up name and add sp to binary tree */
		if (*name == '_') /* Ignore leading '_' */
			++name;
		findsym(name, sp, strncmp);
		if ((sp->u.n.n_type & N_EXT) && (sp->u.n.n_type != N_EXT))
			sp->u.n.n_other = DEF_BY_MODULE; /* abuse: mark extdef */
		else {
			sp->u.n.n_other = 0; /* abuse: mark extref */
		}
		if (sp->u.n.n_type == (N_UNDF | N_EXT) && symvalue( sp) > 0)
			bss2size += symvalue( sp);
	}
	if (verbose)
	        insmod_debug ("external bss size =%7d\n", bss2size);
	codesize = aouthdr->a_text + aouthdr->a_data;
        bss1size = aouthdr->a_bss;
	progsize = codesize + bss1size + bss2size;
        if (verbose)
		insmod_debug ("total module size =%7d", progsize);
        textseg = (char*) ckalloc( progsize);
	memset(textseg, 0, progsize);

	/* read in text and data segments */
	fseek(fp, sizeof(struct exec), SEEK_SET);
	fread(textseg, codesize, 1, fp);
	if (feof(fp) || ferror(fp))
		return "Error reading text & data";
	dataseg = textseg + aouthdr->a_text;
	bss1seg = dataseg + aouthdr->a_data;
	bss2seg = bss1seg + bss1size;
	if (verbose) {
		insmod_debug ("textseg  -> 0x%08lx, size =%7d",
	                (unsigned long)textseg, aouthdr->a_text);
		insmod_debug ("dataseg  -> 0x%08lx, size =%7d",
	                (unsigned long)dataseg, aouthdr->a_data);
		insmod_debug ("bss1seg  -> 0x%08lx, size =%7d",
	                (unsigned long)bss1seg, bss1size);
		insmod_debug ("bss2seg  -> 0x%08lx, size =%7d",
	                (unsigned long)bss2seg, bss2size);
	}
	return (char *)0;
}
