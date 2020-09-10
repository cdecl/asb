
FROM ghcr.io/cdecl/alpine-gcc AS build
WORKDIR /app
RUN git clone https://github.com/cdecl/asb.git && cd asb/src && make -f Makefile.static 


FROM alpine AS run
WORKDIR /app
RUN apk add wrk
COPY --from=build /app/asb/src/asb /usr/bin/
RUN chmod +x /usr/bin/asb
CMD ["asb"]

