FROM skalenetwork/sgxwallet_base:latest

COPY . /usr/src/sdk
WORKDIR /usr/src/sdk
RUN apt update && apt install -y curl secure-delete
RUN touch /var/hwmode
RUN ./autoconf.bash
RUN ./configure
RUN bash -c "make -j$(nproc)"
RUN ccache -sz
RUN mkdir -p /usr/src/sdk/sgx_data
COPY docker/start.sh ./
ENTRYPOINT ["/usr/src/sdk/start.sh"]
