#
# Public domain
# ex:syn=make
#

PROJNAME=	agar
PKGNAME=	${PROJNAME}-${VERSION}-${OS}-${ARCH}${FLAVOR}
SRCDIR=		/home/vedge/src/agar

VERSION=	`(cd ${SRCDIR} && perl mk/get-version.pl)`
RELEASE=	`(cd ${SRCDIR} && perl mk/get-release.pl)`
BUILDDIR=	/tmp/w-${PKGNAME}
FAKEDIR=	/tmp/pkg-${PKGNAME}
RELENGDIR=	${SRCDIR}/Packages/${OS}-${ARCH}${FLAVOR}
PLATFORM?=	unix

all: pre-build do-build package sign

pre-build:
	mkdir ${BUILDDIR}
	mkdir ${FAKEDIR}
	mkdir ${FAKEDIR}/${PKGNAME}

package:
	(cd ${FAKEDIR} && \
	 cp -f ${SRCDIR}/README ${PKGNAME} && \
	 cp -f ${SRCDIR}/mk/LICENSE.txt ${PKGNAME}/LICENSE && \
	 cp -f ${SRCDIR}/gui/fonts/Vera-Copyright.txt ${PKGNAME}/LICENSE-Vera && \
	 cp -f ${SRCDIR}/mk/agar-logo.png ${PKGNAME} && \
	 cp -f ${RELENGDIR}/README.* ${PKGNAME} && \
	 cat ${RELENGDIR}/../install-${PLATFORM}.sh | \
	     perl ${RELENGDIR}/../install-subst.pl "${SRCDIR}" "${PROJNAME}" \
	     "${OS}" "${ARCH}" "${FLAVOR}" > ${PKGNAME}/install && \
	 chmod 755 ${PKGNAME}/install && \
	 cp -f ${RELENGDIR}/agar-config.sh ${PKGNAME} && \
	 tar -f ${PKGNAME}.tar -c ${PKGNAME} && \
	 gzip -f ${PKGNAME}.tar && \
	 cp -f ${PKGNAME}.tar.gz ${RELENGDIR})

sign:
	 gpg -ab ${PKGNAME}.tar.gz

clean:
	rm -fR ${BUILDDIR}
	rm -fR ${FAKEDIR}

.PHONY: pre-build package sign do-build clean