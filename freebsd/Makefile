PORTNAME=	octopkg-tui
DISTVERSIONPREFIX=	v
DISTVERSION=	0.3.1
CATEGORIES=	ports-mgmt

MAINTAINER=	cleber@bsd.com.br
COMMENT=	Terminal front-end for pkg(8), derived from OctoPkg
WWW=		https://github.com/cleberalves40/octopkg-tui

LICENSE=	GPLv2+
LICENSE_FILE=	${WRKSRC}/LICENSE

LIB_DEPENDS=	libftxui-component.so:devel/ftxui

USES=		cmake compiler:c++17-lang
USE_GITHUB=	yes
GH_ACCOUNT=	cleberalves40

SUB_FILES=	pkg-message

PLIST_FILES=	bin/octopkg-tui

.include <bsd.port.mk>
