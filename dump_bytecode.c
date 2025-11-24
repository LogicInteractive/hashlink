#include <stdio.h>
#include <stdlib.h>
#include "src/hl.h"
#include "src/hlmodule.h"

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <file.hl>\n", argv[0]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "Cannot open %s\n", argv[1]);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	fseek(f, 0, SEEK_SET);

	unsigned char *data = malloc(size);
	fread(data, 1, size, f);
	fclose(f);

	char *error = NULL;
	hl_code *code = hl_code_read(data, size, &error);
	if (!code) {
		fprintf(stderr, "Error: %s\n", error ? error : "Unknown");
		return 1;
	}

	printf("=== BYTECODE ANALYSIS ===\n");
	printf("nints: %d\n", code->nints);
	printf("nconstants: %d\n", code->nconstants);
	printf("\n=== INTEGER CONSTANTS ===\n");
	for (int i = 0; i < code->nints; i++) {
		printf("ints[%d] = 0x%08x (%d)\n", i, (unsigned int)code->ints[i], code->ints[i]);
	}

	printf("\n=== OBJECT CONSTANTS ===\n");
	for (int i = 0; i < code->nconstants; i++) {
		hl_constant *c = &code->constants[i];
		hl_type *t = code->types + c->t;
		printf("\nConstant %d: global=%d, type=%d (kind=%d)\n", i, c->global, c->t, t->kind);

		if (t->kind == HOBJ || t->kind == HSTRUCT) {
			printf("  Object/Struct with %d fields:\n", c->nfields);
			for (int j = 0; j < c->nfields; j++) {
				int idx = c->fields[j];
				hl_type *ft = t->obj->fields[j].t;
				printf("    Field %d: idx=%d, type=%d (kind=%d)", j, idx, ft - code->types, ft->kind);
				if (ft->kind == HI32) {
					if (idx < code->nints) {
						printf(" -> ints[%d]=0x%08x", idx, (unsigned int)code->ints[idx]);
					} else {
						printf(" -> INDEX OUT OF RANGE!");
					}
				}
				printf("\n");
			}
		}
	}

	// Dump OField/OSetField opcodes to understand bytecode format
	printf("\n=== FIELD ACCESS OPCODES ===\n");
	for (int fi = 0; fi < code->nfunctions; fi++) {
		hl_function *func = &code->functions[fi];
		for (int oi = 0; oi < func->nops; oi++) {
			hl_opcode *op = &func->ops[oi];
			if (op->op == OField) {
				// OField: p1=dst, p2=obj, p3=field_param
				hl_type *obj_type = func->regs[op->p2];
				printf("F%d op%d OField: dst=r%d obj=r%d(type=%d) p3=%d",
					func->findex, oi, op->p1, op->p2, obj_type->kind, op->p3);
				if (obj_type->kind == HOBJ || obj_type->kind == HSTRUCT) {
					printf(" (obj has %d fields)", obj_type->obj->nfields);
					if (op->p3 < obj_type->obj->nfields) {
						printf(" field_name='%s'", obj_type->obj->fields[op->p3].name ?
							(char*)obj_type->obj->fields[op->p3].name : "<null>");
					}
				}
				printf("\n");
			} else if (op->op == OSetField) {
				// OSetField: p1=obj, p2=field_param, p3=src
				hl_type *obj_type = func->regs[op->p1];
				printf("F%d op%d OSetField: obj=r%d(type=%d) p2=%d src=r%d",
					func->findex, oi, op->p1, obj_type->kind, op->p2, op->p3);
				if (obj_type->kind == HOBJ || obj_type->kind == HSTRUCT) {
					printf(" (obj has %d fields)", obj_type->obj->nfields);
					if (op->p2 < obj_type->obj->nfields) {
						printf(" field_name='%s'", obj_type->obj->fields[op->p2].name ?
							(char*)obj_type->obj->fields[op->p2].name : "<null>");
					}
				}
				printf("\n");
			}
		}
	}

	return 0;
}
