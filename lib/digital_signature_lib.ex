defmodule DigitalSignatureLib do
  @on_load {:init, 0}
  @moduledoc """
  Elixir implementation of pkcs7 data processing that uses uaCrypto library (ICAO version) via the NIF api.
  """

  def init do
    path = :filename.join(priv_dir(), 'digital_signature_lib_nif')
    :ok = :erlang.load_nif(path, 0)
  end

  def priv_dir do
    case :code.priv_dir(:digital_signature_lib) do
      {:error, _} ->
        :code.which(:digital_signature_lib)
        |> :filename.dirname()
        |> :filename.dirname()
        |> :filename.join('priv')

      path ->
        path
    end
  end

  def processPKCS7Data(_, _, _), do: exit(:nif_library_not_loaded)
end
