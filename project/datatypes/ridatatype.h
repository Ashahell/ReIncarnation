/* ridatatype.h — shared datatype registration descriptor (Task 14,
 * fix round 1).
 *
 * AROS-only USE (included only by the two datatype shells under
 * project/datatypes, which never enter the host build); the struct
 * itself is plain C so the header carries no platform guard. One
 * definition: both shells previously defined this struct privately
 * with no shared prototype.
 */
#ifndef RI_DATATYPE_H
#define RI_DATATYPE_H

/* Static facts the OS registration call consumes: node name +
 * filename pattern. The AddDataType call itself is Task-14-deferred
 * (needs the AROS run — method in
 * docs/evidence/formats/beta-exit.md). */
struct RIDatatypeReg {
    const char *dt_name;
    const char *dt_pattern;
};

#endif
